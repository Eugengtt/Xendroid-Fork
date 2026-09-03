/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/cpu/backend/a64/a64_code_cache.h"

#include <cstring>
#include <vector>

#include "xenia/base/assert.h"
#include "xenia/base/logging.h"
#include "xenia/base/math.h"
#include "xenia/base/platform.h"
#include "xenia/cpu/backend/a64/a64_stack_layout.h"
#include "xenia/base/platform.h"

// libgcc/libunwind APIs for registering DWARF .eh_frame unwind info.
// libgcc takes a pointer to a [CIE | FDEs | terminator] section and walks it.
// Apple/LLVM libunwind takes a single FDE pointer and must be called once
// per FDE, so on XE_PLATFORM_MAC we walk the buffer ourselves.
extern "C" void __register_frame(void*);
extern "C" void __deregister_frame(void*);

#if XE_PLATFORM_xendroid
#include "../aarch64_disasm.h"
#include <unwind.h>
extern "C" _Unwind_Reason_Code __gxx_personality_v0(
        int version,
        _Unwind_Action actions,
        uint64_t exceptionClass,
        _Unwind_Exception* exceptionObject,
        _Unwind_Context* context);

static _Unwind_Reason_Code trace(struct _Unwind_Context* ctx,void*){
    uint64_t ip=reinterpret_cast<uint64_t>(_Unwind_GetIP(ctx));
    XELOGI("FRAME: {:16X}",ip);
    std::string insts= aarch64_disasm(ip,reinterpret_cast<uint32_t*>(ip),16);
    XELOGI(insts);
    return _URC_NO_REASON;
}

static _Unwind_Reason_Code __jit_personality(
        int version,
        _Unwind_Action actions,
        uint64_t exceptionClass,
        _Unwind_Exception* exceptionObject,
        _Unwind_Context* context){
    if(actions&_UA_CLEANUP_PHASE){
        XELOGI("_UA_CLEANUP_PHASE IPs={:16X}",reinterpret_cast<uint64_t>(_Unwind_GetIP(context)));
        _Unwind_Backtrace(trace,nullptr);
    }
    return __gxx_personality_v0(version,actions,exceptionClass,exceptionObject,context);
}
#endif
namespace xe {
namespace cpu {
namespace backend {
namespace a64 {

// Maximum size of DWARF .eh_frame data per function (CIE + FDE + terminator).
static constexpr uint32_t kMaxUnwindInfoSize = 128;

// DWARF register numbers for AArch64.
static constexpr uint8_t kDwarfRegX19 = 19;
static constexpr uint8_t kDwarfRegX20 = 20;
static constexpr uint8_t kDwarfRegX21 = 21;
static constexpr uint8_t kDwarfRegX22 = 22;
static constexpr uint8_t kDwarfRegX23 = 23;
static constexpr uint8_t kDwarfRegX24 = 24;
static constexpr uint8_t kDwarfRegX25 = 25;
static constexpr uint8_t kDwarfRegX26 = 26;
static constexpr uint8_t kDwarfRegX27 = 27;
static constexpr uint8_t kDwarfRegX28 = 28;
static constexpr uint8_t kDwarfRegFP = 29;  // x29 / frame pointer
static constexpr uint8_t kDwarfRegLR = 30;  // x30 / link register
static constexpr uint8_t kDwarfRegSP = 31;  // stack pointer
static constexpr uint8_t kDwarfRegD8 = 72;  // d8-d15 are callee-saved
static constexpr uint8_t kDwarfRegD9 = 73;
static constexpr uint8_t kDwarfRegD10 = 74;
static constexpr uint8_t kDwarfRegD11 = 75;
static constexpr uint8_t kDwarfRegD12 = 76;
static constexpr uint8_t kDwarfRegD13 = 77;
static constexpr uint8_t kDwarfRegD14 = 78;
static constexpr uint8_t kDwarfRegD15 = 79;

// DWARF CFA opcodes.
static constexpr uint8_t kDW_CFA_advance_loc1 = 0x02;
static constexpr uint8_t kDW_CFA_advance_loc2 = 0x03;
static constexpr uint8_t kDW_CFA_advance_loc4 = 0x04;
static constexpr uint8_t kDW_CFA_offset_extended = 0x05;
static constexpr uint8_t kDW_CFA_def_cfa = 0x0c;
static constexpr uint8_t kDW_CFA_def_cfa_offset = 0x0e;
static constexpr uint8_t kDW_CFA_nop = 0x00;

// DWARF pointer encoding constants.
static constexpr uint8_t kDW_EH_PE_pcrel = 0x10;
static constexpr uint8_t kDW_EH_PE_sdata4 = 0x0b;
static constexpr uint8_t kDW_EH_PE_udata8 = 0x04;
static constexpr uint8_t kDW_EH_PE_absptr = 0x00;



static size_t WriteULEB128(uint8_t* p, uint64_t value) {
  size_t count = 0;
  do {
    uint8_t byte = value & 0x7F;
    value >>= 7;
    if (value) {
      byte |= 0x80;
    }
    p[count++] = byte;
  } while (value);
  return count;
}

static std::vector<uint8_t> encode_uleb128(uint64_t value) {
  // WriteULEB128 writes the bytes into the buffer while it computes the length,
  // so it needs a valid buffer up front. The previous code passed
  // result.data() of the still-empty vector (a null pointer), so WriteULEB128
  // wrote through nullptr -> SIGSEGV at address 0 on the very first JIT code
  // placement, hanging the emulator before any game could boot.
  // A 64-bit value encodes to at most 10 ULEB128 bytes.
  uint8_t tmp[10];
  size_t count = WriteULEB128(tmp, value);
  return std::vector<uint8_t>(tmp, tmp + count);
}

static size_t WriteSLEB128(uint8_t* p, int64_t value) {
  size_t count = 0;
  bool more = true;
  while (more) {
    uint8_t byte = value & 0x7F;
    value >>= 7;
    if ((value == 0 && !(byte & 0x40)) || (value == -1 && (byte & 0x40))) {
      more = false;
    } else {
      byte |= 0x80;
    }
    p[count++] = byte;
  }
  return count;
}

class PosixA64CodeCache : public A64CodeCache {
 public:
  PosixA64CodeCache();
  ~PosixA64CodeCache() override;

  bool Initialize() override;

  void* LookupUnwindInfo(uint64_t host_pc) override { return nullptr; }

 private:
  UnwindReservation RequestUnwindReservation(uint8_t* entry_address) override;
  void PlaceCode(uint32_t guest_address, void* machine_code,
                 const EmitFunctionInfo& func_info, void* code_execute_address,
                 UnwindReservation unwind_reservation) override;

  void InitializeUnwindEntry(uint8_t* unwind_entry_address,
                             void* code_execute_address,
                             const EmitFunctionInfo& func_info);

  std::vector<void*> registered_frames_;
  uint32_t unwind_table_count_ = 0;

#if XE_PLATFORM_xendroid
  // Bump allocator for the JIT's DWARF .eh_frame (a CIE+FDE per function).
  // eh_frame_table_ is the current write cursor; _base_/_end_ bound the
  // allocation so the cursor can be size-checked and the buffer freed correctly
  // (the cursor is advanced past the base, so it must not itself be delete[]d).
  uint8_t* eh_frame_table_ = nullptr;
  uint8_t* eh_frame_table_base_ = nullptr;
  uint8_t* eh_frame_table_end_ = nullptr;
#endif
};

std::unique_ptr<A64CodeCache> A64CodeCache::Create() {
  return std::make_unique<PosixA64CodeCache>();
}

PosixA64CodeCache::PosixA64CodeCache() = default;

PosixA64CodeCache::~PosixA64CodeCache() {
  for (auto frame : registered_frames_) {
    __deregister_frame(frame);
  }

#if XE_PLATFORM_xendroid
  // Free the original allocation, not the advanced write cursor.
  delete[] eh_frame_table_base_;
#endif
}

bool PosixA64CodeCache::Initialize() {
  if (!A64CodeCache::Initialize()) {
    return false;
  }

#if XE_PLATFORM_xendroid
  // 128 MiB. Worst case kMaximumFunctionCount (1,000,000) x ~70 B/entry
  // (CIE+FDE) ~= 70 MB; 128 MiB leaves headroom, and the per-write bounds check
  // in InitializeUnwindEntry turns exhaustion into a clean abort rather than a
  // silent heap overflow.
  constexpr size_t kEhFrameTableSize = size_t(128) * 1024 * 1024;
  eh_frame_table_base_ = new uint8_t[kEhFrameTableSize];
  eh_frame_table_ = eh_frame_table_base_;
  eh_frame_table_end_ = eh_frame_table_base_ + kEhFrameTableSize;
  if(reinterpret_cast<uint64_t>(eh_frame_table_)%4!=0){
      xe::FatalError("Unwind table is not 4-byte aligned!");
  }
#endif

  registered_frames_.reserve(kMaximumFunctionCount);
  return true;
}

A64CodeCache::UnwindReservation PosixA64CodeCache::RequestUnwindReservation(
    uint8_t* entry_address) {
#if defined(NDEBUG)
  if (unwind_table_count_ >= kMaximumFunctionCount) {
    xe::FatalError(
        "Unwind table count exceeded maximum! Please report this to "
        "Xenia developers");
  }
#else
  assert_false(unwind_table_count_ >= kMaximumFunctionCount);
#endif
  UnwindReservation unwind_reservation;
#if XE_PLATFORM_xendroid
  // On xendroid the DWARF .eh_frame lives in a separate bump buffer
  // (eh_frame_table_); InitializeUnwindEntry/PlaceCode never write to or
  // register the per-function tail in the code region (unlike the #else path,
  // which anchors pcrel pointers there). Reserving a tail here only wastes code
  // space and forces a redundant trap-fill + a second I-cache flush over a
  // region that is never read or executed.
  unwind_reservation.data_size = 0;
#else
  unwind_reservation.data_size = xe::round_up(kMaxUnwindInfoSize, 16);
#endif
  unwind_reservation.table_slot = unwind_table_count_++;
  unwind_reservation.entry_address = entry_address;
  return unwind_reservation;
}

void PosixA64CodeCache::PlaceCode(uint32_t guest_address, void* machine_code,
                                  const EmitFunctionInfo& func_info,
                                  void* code_execute_address,
                                  UnwindReservation unwind_reservation) {
  InitializeUnwindEntry(unwind_reservation.entry_address, code_execute_address,
                        func_info);

#if XE_PLATFORM_xendroid
  // On xendroid the DWARF .eh_frame lives in the separate eh_frame_table_ bump
  // buffer and each FDE is registered from InitializeUnwindEntry, so there is
  // nothing to register here (and the per-function tail in the code region is
  // never written).
  (void)unwind_reservation;
#else
  uint8_t* unwind_execute_address = unwind_reservation.entry_address -
                                    generated_code_write_base_ +
                                    generated_code_execute_base_;
#if XE_PLATFORM_MAC
  // Walk [CIE | FDE | terminator] and register each FDE individually.
  const uint8_t* p = unwind_reservation.entry_address;
  uint8_t* p_execute = unwind_execute_address;
  while (true) {
    uint32_t length = *reinterpret_cast<const uint32_t*>(p);
    if (length == 0) {
      break;
    }
    uint32_t cie_id_or_ptr = *reinterpret_cast<const uint32_t*>(p + 4);
    if (cie_id_or_ptr != 0) {
      __register_frame(p_execute);
      registered_frames_.push_back(p_execute);
    }
    p += 4 + length;
    p_execute += 4 + length;
  }
#else
  __register_frame(unwind_execute_address);
  registered_frames_.push_back(unwind_execute_address);
#endif  // XE_PLATFORM_MAC
#endif  // XE_PLATFORM_xendroid
}

void PosixA64CodeCache::InitializeUnwindEntry(
    uint8_t* unwind_entry_address, void* code_execute_address,
    const EmitFunctionInfo& func_info) {
  // Compute execute-side base address of the unwind buffer.
  uint8_t* unwind_execute_base = unwind_entry_address -
                                 generated_code_write_base_ +
                                 generated_code_execute_base_;

  uint8_t* p = eh_frame_table_;
  uint8_t* cie_start = p;
#if XE_PLATFORM_xendroid

    // Refuse to bump past the end of the table instead of corrupting the heap.
    // Real entries are <= ~140 B (a thunk's CIE+FDE); 1 KiB is a safe ceiling.
    if (eh_frame_table_ + 1024 > eh_frame_table_end_) {
      xe::FatalError(
          "A64 unwind table exhausted: this session compiled more JIT code "
          "than the eh_frame table can hold. Increase kEhFrameTableSize.");
    }

    struct cie_t{
        uint32_t len;
        uint32_t id;
        uint8_t version; //1 or 3
        uint8_t augmentation[5]; //zPLR\0
        uint8_t code_alignment_factor;
        uint8_t data_alignment_factor;
        uint8_t return_address_register;
        uint8_t augmentation_data_size; //
        uint8_t augmentation_data[3+8];
        uint8_t program[3];
    } cie={
            .len=sizeof(cie_t)-4,
            .id=0,
            .version=1,
            .augmentation={'z','P','L','R','\0'},
            .code_alignment_factor=1,//ULEB128 1
            .data_alignment_factor=0x78,//SLEB128 -8
            .return_address_register=30,
            .augmentation_data_size=11,
            .augmentation_data={
                kDW_EH_PE_absptr|kDW_EH_PE_udata8,
                0,0,0,0,0,0,0,0,
                kDW_EH_PE_pcrel|kDW_EH_PE_sdata4,
                kDW_EH_PE_absptr|kDW_EH_PE_udata8,
            },
            .program={
                    kDW_CFA_def_cfa,31,0,//DW_CFA_def_cfa: reg31(SP) +0
            }

    };

    {
        uint64_t p__jit_personality=reinterpret_cast<uint64_t>(__gxx_personality_v0);
        memcpy(&cie.augmentation_data[0+1],&p__jit_personality,8);
    }

    memcpy(p,&cie, sizeof(cie_t));
    p+=sizeof(cie_t);

    struct fde_t{
        uint32_t len;
        uint32_t cie_pointer;
        uint64_t pc_start;
        uint64_t pc_range;
        uint8_t augmentation_data_size;
        uint8_t augmentation_data[4];
        uint8_t program[0];//pad 3
    } fde={
            .len=sizeof(fde_t)-4,
            .cie_pointer=4+cie.len+4,
            .pc_start=reinterpret_cast<uint64_t>(code_execute_address),
            .pc_range=func_info.code_size.total,
            .augmentation_data_size=4,
            .augmentation_data={0,0,0,0,},//LSDA
            .program={}
    };
    std::vector<uint8_t> fde_program;
    constexpr uint8_t DW_CFA_advance_loc = 0x40;
// FDE instructions.
    if (func_info.stack_size > 0) {
        // Advance to the instruction after the stack allocation.
        size_t alloc_offset = func_info.prolog_stack_alloc_offset;
        if (alloc_offset > 0) {
            // ARM64 code alignment factor is 4, so divide by 4.
            uint32_t factored_offset = alloc_offset;
            if (factored_offset < 64) {
                fde_program.push_back(DW_CFA_advance_loc | static_cast<uint8_t>(factored_offset));
            } else if (factored_offset < 256) {
                fde_program.push_back(kDW_CFA_advance_loc1);
                fde_program.push_back(static_cast<uint8_t>(factored_offset));
            } else if (factored_offset < 65536){
                fde_program.push_back(kDW_CFA_advance_loc2);
                uint16_t factored_offset_u16 = static_cast<uint16_t>(factored_offset);
                //little endian
                fde_program.push_back(*(reinterpret_cast<uint8_t*>(&factored_offset_u16)+0));
                fde_program.push_back(*(reinterpret_cast<uint8_t*>(&factored_offset_u16)+1));
            }
            else {
                fde_program.push_back(kDW_CFA_advance_loc4);
                //little endian
                fde_program.push_back(*(reinterpret_cast<uint8_t*>(&factored_offset)+0));
                fde_program.push_back(*(reinterpret_cast<uint8_t*>(&factored_offset)+1));
                fde_program.push_back(*(reinterpret_cast<uint8_t*>(&factored_offset)+2));
                fde_program.push_back(*(reinterpret_cast<uint8_t*>(&factored_offset)+3));
            }
        }

        // DW_CFA_def_cfa_offset: CFA = SP + stack_size.
        fde_program.push_back(kDW_CFA_def_cfa_offset);
        //p += WriteULEB128(p, func_info.stack_size);
        std::vector uleb128_stack_size = encode_uleb128(func_info.stack_size);
        fde_program.insert(fde_program.end(), uleb128_stack_size.begin(), uleb128_stack_size.end());

        if (func_info.stack_size == StackLayout::THUNK_STACK_SIZE) {
            // Thunk: encode all callee-saved register save locations.
            // See a64_stack_layout.h for the layout.
            size_t cfa = func_info.stack_size;  // 224

            // GPRs: x19-x28 saved as stp pairs at sp+0x00..0x48
            constexpr uint8_t DW_CFA_offset=0x80;
            fde_program.push_back(DW_CFA_offset | kDwarfRegX19);
            std::vector uleb128_x19_offset = encode_uleb128((func_info.stack_size - 0x000)/8);
            fde_program.insert(fde_program.end(), uleb128_x19_offset.begin(), uleb128_x19_offset.end());
            fde_program.push_back(DW_CFA_offset | kDwarfRegX20);
            std::vector uleb128_x20_offset = encode_uleb128((func_info.stack_size - 0x008)/8);
            fde_program.insert(fde_program.end(), uleb128_x20_offset.begin(), uleb128_x20_offset.end());
            fde_program.push_back(DW_CFA_offset | kDwarfRegX21);
            std::vector uleb128_x21_offset = encode_uleb128((func_info.stack_size - 0x010)/8);
            fde_program.insert(fde_program.end(), uleb128_x21_offset.begin(), uleb128_x21_offset.end());
            fde_program.push_back(DW_CFA_offset | kDwarfRegX22);
            std::vector uleb128_x22_offset = encode_uleb128((func_info.stack_size - 0x018)/8);
            fde_program.insert(fde_program.end(), uleb128_x22_offset.begin(), uleb128_x22_offset.end());
            fde_program.push_back(DW_CFA_offset | kDwarfRegX23);
            std::vector uleb128_x23_offset = encode_uleb128((func_info.stack_size - 0x020)/8);
            fde_program.insert(fde_program.end(), uleb128_x23_offset.begin(), uleb128_x23_offset.end());
            fde_program.push_back(DW_CFA_offset | kDwarfRegX24);
            std::vector uleb128_x24_offset = encode_uleb128((func_info.stack_size - 0x028)/8);
            fde_program.insert(fde_program.end(), uleb128_x24_offset.begin(), uleb128_x24_offset.end());
            fde_program.push_back(DW_CFA_offset | kDwarfRegX25);
            std::vector uleb128_x25_offset = encode_uleb128((func_info.stack_size - 0x030)/8);
            fde_program.insert(fde_program.end(), uleb128_x25_offset.begin(), uleb128_x25_offset.end());
            fde_program.push_back(DW_CFA_offset | kDwarfRegX26);
            std::vector uleb128_x26_offset = encode_uleb128((func_info.stack_size - 0x038)/8);
            fde_program.insert(fde_program.end(), uleb128_x26_offset.begin(), uleb128_x26_offset.end());
            fde_program.push_back(DW_CFA_offset | kDwarfRegX27);
            std::vector uleb128_x27_offset = encode_uleb128((func_info.stack_size - 0x040)/8);
            fde_program.insert(fde_program.end(), uleb128_x27_offset.begin(), uleb128_x27_offset.end());
            fde_program.push_back(DW_CFA_offset | kDwarfRegX28);
            std::vector uleb128_x28_offset = encode_uleb128((func_info.stack_size - 0x048)/8);
            fde_program.insert(fde_program.end(), uleb128_x28_offset.begin(), uleb128_x28_offset.end());
            fde_program.push_back(DW_CFA_offset | kDwarfRegFP);
            std::vector uleb128_fp_offset = encode_uleb128((func_info.stack_size - 0x050)/8);
            fde_program.insert(fde_program.end(), uleb128_fp_offset.begin(), uleb128_fp_offset.end());
            fde_program.push_back(DW_CFA_offset | kDwarfRegLR);
            std::vector uleb128_lr_offset = encode_uleb128((func_info.stack_size - 0x058)/8);
            fde_program.insert(fde_program.end(), uleb128_lr_offset.begin(), uleb128_lr_offset.end());
            // NEON: d8-d15 saved as full q8-q15 via stp pairs at sp+0x060..0xDF.
            // Each Q is 16 bytes; d8-d15 are the low 64 bits of q8-q15.
            // stp q8,q9 at sp+0x060: d8=sp+0x060, d9=sp+0x070
            // stp q10,q11 at sp+0x080: d10=sp+0x080, d11=sp+0x090
            // stp q12,q13 at sp+0x0A0: d12=sp+0x0A0, d13=sp+0x0B0
            // stp q14,q15 at sp+0x0C0: d14=sp+0x0C0, d15=sp+0x0D0
            constexpr uint8_t DW_CFA_offset_extended=0x05;
            fde_program.push_back(DW_CFA_offset_extended);
            fde_program.push_back(kDwarfRegD8);
            std::vector uleb128_d8_offset = encode_uleb128((func_info.stack_size - 0x060)/8);
            fde_program.insert(fde_program.end(), uleb128_d8_offset.begin(), uleb128_d8_offset.end());
            fde_program.push_back(DW_CFA_offset_extended);
            fde_program.push_back(kDwarfRegD9);
            std::vector uleb128_d9_offset = encode_uleb128((func_info.stack_size - 0x070)/8);
            fde_program.insert(fde_program.end(), uleb128_d9_offset.begin(), uleb128_d9_offset.end());
            fde_program.push_back(DW_CFA_offset_extended);
            fde_program.push_back(kDwarfRegD10);
            std::vector uleb128_d10_offset = encode_uleb128((func_info.stack_size - 0x080)/8);
            fde_program.insert(fde_program.end(), uleb128_d10_offset.begin(), uleb128_d10_offset.end());
            fde_program.push_back(DW_CFA_offset_extended);
            fde_program.push_back(kDwarfRegD11);
            std::vector uleb128_d11_offset = encode_uleb128((func_info.stack_size - 0x090)/8);
            fde_program.insert(fde_program.end(), uleb128_d11_offset.begin(), uleb128_d11_offset.end());
            fde_program.push_back(DW_CFA_offset_extended);
            fde_program.push_back(kDwarfRegD12);
            std::vector uleb128_d12_offset = encode_uleb128((func_info.stack_size - 0x0A0)/8);
            fde_program.insert(fde_program.end(), uleb128_d12_offset.begin(), uleb128_d12_offset.end());
            fde_program.push_back(DW_CFA_offset_extended);
            fde_program.push_back(kDwarfRegD13);
            std::vector uleb128_d13_offset = encode_uleb128((func_info.stack_size - 0x0B0)/8);
            fde_program.insert(fde_program.end(), uleb128_d13_offset.begin(), uleb128_d13_offset.end());
            fde_program.push_back(DW_CFA_offset_extended);
            fde_program.push_back(kDwarfRegD14);
            std::vector uleb128_d14_offset = encode_uleb128((func_info.stack_size - 0x0C0)/8);
            fde_program.insert(fde_program.end(), uleb128_d14_offset.begin(), uleb128_d14_offset.end());
            fde_program.push_back(DW_CFA_offset_extended);
            fde_program.push_back(kDwarfRegD15);
            std::vector uleb128_d15_offset = encode_uleb128((func_info.stack_size - 0x0D0)/8);
            fde_program.insert(fde_program.end(), uleb128_d15_offset.begin(), uleb128_d15_offset.end());

        } else if (func_info.lr_save_offset > 0) {
            // Record where x30 (LR / return address) is saved.
            // Without this, the unwinder cannot find the return address.
            fde_program.push_back(0x80 | kDwarfRegLR);
            std::vector uleb128_lr_offset = encode_uleb128((func_info.stack_size - func_info.lr_save_offset)/8);
            fde_program.insert(fde_program.end(), uleb128_lr_offset.begin(), uleb128_lr_offset.end());
        }
    }

    // Pad FDE.

    int len=sizeof(fde_t)+fde_program.size()-3-4;
    int pad=len%4;
    len+=pad;
    for(;pad>0;pad--){
        fde_program.push_back(kDW_CFA_nop);
    }
    fde.len=len;
    memcpy(p,&fde,sizeof(fde_t));
    p+=sizeof(fde_t)-3;
    memcpy(p,fde_program.data(),fde_program.size());
    p+=fde_program.size();
    // Android's LLVM libunwind __register_frame() expects a pointer to the FDE
    // (libgcc instead takes the .eh_frame/CIE start). The CIE is at
    // eh_frame_table_ and the FDE immediately follows it. Passing the CIE made
    // libunwind reject the entry (a CIE's CIE-id field is 0, so it is not a
    // valid FDE) -> no unwind info for JIT frames -> XThread::Reenter's
    // FiberReentryException could not unwind through them -> std::terminate.
    uint8_t* fde_address = eh_frame_table_ + sizeof(cie_t);
    __register_frame(fde_address);
    registered_frames_.push_back(fde_address);
    eh_frame_table_=p;

#else
  // === CIE (Common Information Entry) ===
  uint8_t* cie_length_ptr = p;
  p += 4;

  uint8_t* cie_content_start = p;

  // CIE ID = 0.
  *reinterpret_cast<uint32_t*>(p) = 0;
  p += 4;

  // Version = 1.
  *p++ = 1;

  // Augmentation string "zR".
  *p++ = 'z';
  *p++ = 'R';
  *p++ = '\0';

  // Code alignment factor = 4 (ARM64 instructions are 4 bytes).
  p += WriteULEB128(p, 4);

  // Data alignment factor = -8.
  p += WriteSLEB128(p, -8);

  // Return address register = x30 (LR).
  p += WriteULEB128(p, kDwarfRegLR);

  // Augmentation data length = 1.
  p += WriteULEB128(p, 1);

  // FDE pointer encoding: pc-relative, signed 32-bit.
  *p++ = kDW_EH_PE_pcrel | kDW_EH_PE_sdata4;

  // Initial instructions:
  // DW_CFA_def_cfa SP, 0 — at function entry, CFA = SP.
  *p++ = kDW_CFA_def_cfa;
  p += WriteULEB128(p, kDwarfRegSP);
  p += WriteULEB128(p, 0);

  // Pad CIE to pointer-size (8-byte) alignment.
  size_t cie_content_len = static_cast<size_t>(p - cie_content_start);
  size_t cie_padded_len = xe::round_up(cie_content_len, sizeof(void*));
  while (p < cie_content_start + cie_padded_len) {
    *p++ = kDW_CFA_nop;
  }

  *reinterpret_cast<uint32_t*>(cie_length_ptr) =
      static_cast<uint32_t>(p - cie_content_start);

  // === FDE (Frame Description Entry) ===
  uint8_t* fde_length_ptr = p;
  p += 4;

  uint8_t* fde_content_start = p;

  // CIE pointer.
  *reinterpret_cast<uint32_t*>(p) = static_cast<uint32_t>(p - cie_start);
  p += 4;

  // PC begin (pc-relative).
  uint8_t* pc_begin_execute_addr =
      unwind_execute_base + (p - unwind_entry_address);
  *reinterpret_cast<int32_t*>(p) =
      static_cast<int32_t>(reinterpret_cast<intptr_t>(code_execute_address) -
                           reinterpret_cast<intptr_t>(pc_begin_execute_addr));
  p += 4;

  // PC range.
  *reinterpret_cast<uint32_t*>(p) =
      static_cast<uint32_t>(func_info.code_size.total);
  p += 4;

  // Augmentation data length = 0.
  p += WriteULEB128(p, 0);

  // FDE instructions.
  if (func_info.stack_size > 0) {
    // Advance to the instruction after the stack allocation.
    size_t alloc_offset = func_info.prolog_stack_alloc_offset;
    if (alloc_offset > 0) {
      // ARM64 code alignment factor is 4, so divide by 4.
      uint32_t factored_offset = static_cast<uint32_t>(alloc_offset / 4);
      if (factored_offset < 64) {
        *p++ = 0x40 | static_cast<uint8_t>(factored_offset);
      } else if (factored_offset < 256) {
        *p++ = kDW_CFA_advance_loc1;
        *p++ = static_cast<uint8_t>(factored_offset);
      } else {
        *p++ = kDW_CFA_advance_loc2;
        *reinterpret_cast<uint16_t*>(p) =
            static_cast<uint16_t>(factored_offset);
        p += 2;
      }
    }

    // DW_CFA_def_cfa_offset: CFA = SP + stack_size.
    *p++ = kDW_CFA_def_cfa_offset;
    p += WriteULEB128(p, func_info.stack_size);

    if (func_info.stack_size == StackLayout::THUNK_STACK_SIZE) {
      // Thunk: encode all callee-saved register save locations.
      // See a64_stack_layout.h for the layout.
      size_t cfa = func_info.stack_size;  // 224

      // GPRs: x19-x28 saved as stp pairs at sp+0x00..0x48
      *p++ = 0x80 | kDwarfRegX19;
      p += WriteULEB128(p, (cfa - 0x000) / 8);
      *p++ = 0x80 | kDwarfRegX20;
      p += WriteULEB128(p, (cfa - 0x008) / 8);
      *p++ = 0x80 | kDwarfRegX21;
      p += WriteULEB128(p, (cfa - 0x010) / 8);
      *p++ = 0x80 | kDwarfRegX22;
      p += WriteULEB128(p, (cfa - 0x018) / 8);
      *p++ = 0x80 | kDwarfRegX23;
      p += WriteULEB128(p, (cfa - 0x020) / 8);
      *p++ = 0x80 | kDwarfRegX24;
      p += WriteULEB128(p, (cfa - 0x028) / 8);
      *p++ = 0x80 | kDwarfRegX25;
      p += WriteULEB128(p, (cfa - 0x030) / 8);
      *p++ = 0x80 | kDwarfRegX26;
      p += WriteULEB128(p, (cfa - 0x038) / 8);
      *p++ = 0x80 | kDwarfRegX27;
      p += WriteULEB128(p, (cfa - 0x040) / 8);
      *p++ = 0x80 | kDwarfRegX28;
      p += WriteULEB128(p, (cfa - 0x048) / 8);
      // x29 (FP) and x30 (LR) at sp+0x050, sp+0x058
      *p++ = 0x80 | kDwarfRegFP;
      p += WriteULEB128(p, (cfa - 0x050) / 8);
      *p++ = 0x80 | kDwarfRegLR;
      p += WriteULEB128(p, (cfa - 0x058) / 8);
      // NEON: d8-d15 saved as full q8-q15 via stp pairs at sp+0x060..0xDF.
      // Each Q is 16 bytes; d8-d15 are the low 64 bits of q8-q15.
      // stp q8,q9 at sp+0x060: d8=sp+0x060, d9=sp+0x070
      // stp q10,q11 at sp+0x080: d10=sp+0x080, d11=sp+0x090
      // stp q12,q13 at sp+0x0A0: d12=sp+0x0A0, d13=sp+0x0B0
      // stp q14,q15 at sp+0x0C0: d14=sp+0x0C0, d15=sp+0x0D0
      // d8-d15 are DWARF regs 72-79; the 0x80|reg shorthand only fits regs
      // < 64, so encode them with DW_CFA_offset_extended (op, regULEB,
      // offULEB).
      auto emit_offset_extended = [&p](uint8_t reg, uint64_t factored_offset) {
        *p++ = kDW_CFA_offset_extended;
        p += WriteULEB128(p, reg);
        p += WriteULEB128(p, factored_offset);
      };
      emit_offset_extended(kDwarfRegD8, (cfa - 0x060) / 8);
      emit_offset_extended(kDwarfRegD9, (cfa - 0x070) / 8);
      emit_offset_extended(kDwarfRegD10, (cfa - 0x080) / 8);
      emit_offset_extended(kDwarfRegD11, (cfa - 0x090) / 8);
      emit_offset_extended(kDwarfRegD12, (cfa - 0x0A0) / 8);
      emit_offset_extended(kDwarfRegD13, (cfa - 0x0B0) / 8);
      emit_offset_extended(kDwarfRegD14, (cfa - 0x0C0) / 8);
      emit_offset_extended(kDwarfRegD15, (cfa - 0x0D0) / 8);
    } else if (func_info.lr_save_offset > 0) {
      // Record where x30 (LR / return address) is saved.
      // Without this, the unwinder cannot find the return address.
      *p++ = 0x80 | kDwarfRegLR;
      p += WriteULEB128(p,
                        (func_info.stack_size - func_info.lr_save_offset) / 8);
    }
  }

  // Pad FDE.
  size_t fde_content_len = static_cast<size_t>(p - fde_content_start);
  size_t fde_padded_len = xe::round_up(fde_content_len, sizeof(void*));
  while (p < fde_content_start + fde_padded_len) {
    *p++ = kDW_CFA_nop;
  }

  *reinterpret_cast<uint32_t*>(fde_length_ptr) =
      static_cast<uint32_t>(p - fde_content_start);

  // === Terminator ===
  *reinterpret_cast<uint32_t*>(p) = 0;
  p += 4;
#endif
#if !XE_PLATFORM_xendroid
  // The xendroid path writes unwind data into its own 64 MiB eh_frame_table_
  // bump buffer (p starts at eh_frame_table_, not at unwind_entry_address, and
  // the per-function unwind_entry_address reservation is unused). So this
  // bound compares unrelated pointers (heap vs. code-cache mmap) and does not
  // apply to it.
  assert_true(static_cast<size_t>(p - unwind_entry_address) <=
              kMaxUnwindInfoSize);
#endif
}

}  // namespace a64
}  // namespace backend
}  // namespace cpu
}  // namespace xe
