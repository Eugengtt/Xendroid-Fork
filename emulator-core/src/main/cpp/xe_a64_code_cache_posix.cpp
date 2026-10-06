/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#include "xenia/cpu/backend/a64/a64_code_cache.h"
#include "xenia/base/logging.h"
#include "aarch64_disasm.h"
#include <unwind.h>
#include <android/log.h>

extern "C" void __clear_cache(void* start, void* end);
extern "C" void __register_frame(void* eh_frame);
extern "C" void __deregister_frame(void* eh_frame);
extern "C" _Unwind_Reason_Code __gxx_personality_v0(
        int version,
        _Unwind_Action actions,
        uint64_t exceptionClass,
        _Unwind_Exception* exceptionObject,
        _Unwind_Context* context);
enum {
    // Call frame instruction encodings.
    DW_CFA_nop = 0x00,
    DW_CFA_set_loc = 0x01,
    DW_CFA_advance_loc1 = 0x02,
    DW_CFA_advance_loc2 = 0x03,
    DW_CFA_advance_loc4 = 0x04,
    DW_CFA_offset_extended = 0x05,
    DW_CFA_restore_extended = 0x06,
    DW_CFA_undefined = 0x07,
    DW_CFA_same_value = 0x08,
    DW_CFA_register = 0x09,
    DW_CFA_remember_state = 0x0a,
    DW_CFA_restore_state = 0x0b,
    DW_CFA_def_cfa = 0x0c,
    DW_CFA_def_cfa_register = 0x0d,
    DW_CFA_def_cfa_offset = 0x0e,

    DW_CFA_advance_loc = 0x40,
    DW_CFA_offset = 0x80,
    DW_CFA_restore = 0xc0,

    // New in DWARF v3:
    DW_CFA_def_cfa_expression = 0x0f,
    DW_CFA_expression = 0x10,
    DW_CFA_offset_extended_sf = 0x11,
    DW_CFA_def_cfa_sf = 0x12,
    DW_CFA_def_cfa_offset_sf = 0x13,
    DW_CFA_val_offset = 0x14,
    DW_CFA_val_offset_sf = 0x15,
    DW_CFA_val_expression = 0x16,

    // Vendor extensions:
    //    HANDLE_DW_CFA_PRED(0x1d, MIPS_advance_loc8, SELECT_MIPS64)
    //    HANDLE_DW_CFA_PRED(0x2d, GNU_window_save, SELECT_SPARC)
    //    HANDLE_DW_CFA_PRED(0x2d, AARCH64_negate_ra_state, SELECT_AARCH64)
    //    HANDLE_DW_CFA_PRED(0x2e, GNU_args_size, SELECT_X86)
    DW_CFA_MIPS_advance_loc8 = 0x1d,
    DW_CFA_GNU_window_save = 0x2d,
    DW_CFA_AARCH64_negate_ra_state = 0x2d,
    DW_CFA_GNU_args_size = 0x2e,

    // Heterogeneous Debugging Extension defined at
    // https://llvm.org/docs/AMDGPUDwarfExtensionsForHeterogeneousDebugging.html#cfa-definition-instructions
    //    HANDLE_DW_CFA(0x30, LLVM_def_aspace_cfa)
    //    HANDLE_DW_CFA(0x31, LLVM_def_aspace_cfa_sf)
    DW_CFA_LLVM_def_aspace_cfa = 0x30,
    DW_CFA_LLVM_def_aspace_cfa_sf = 0x31,

};

enum {
    // Children flag
    DW_CHILDREN_no = 0x00,
    DW_CHILDREN_yes = 0x01,

    DW_EH_PE_absptr = 0x00,
    DW_EH_PE_omit = 0xff,
    DW_EH_PE_uleb128 = 0x01,
    DW_EH_PE_udata2 = 0x02,
    DW_EH_PE_udata4 = 0x03,
    DW_EH_PE_udata8 = 0x04,
    DW_EH_PE_sleb128 = 0x09,
    DW_EH_PE_sdata2 = 0x0A,
    DW_EH_PE_sdata4 = 0x0B,
    DW_EH_PE_sdata8 = 0x0C,
    DW_EH_PE_signed = 0x08,
    DW_EH_PE_pcrel = 0x10,
    DW_EH_PE_textrel = 0x20,
    DW_EH_PE_datarel = 0x30,
    DW_EH_PE_funcrel = 0x40,
    DW_EH_PE_aligned = 0x50,
    DW_EH_PE_indirect = 0x80
};

// 64-bit ARM64 registers
enum {
    UNW_AARCH64_X0 = 0,
    UNW_AARCH64_X1 = 1,
    UNW_AARCH64_X2 = 2,
    UNW_AARCH64_X3 = 3,
    UNW_AARCH64_X4 = 4,
    UNW_AARCH64_X5 = 5,
    UNW_AARCH64_X6 = 6,
    UNW_AARCH64_X7 = 7,
    UNW_AARCH64_X8 = 8,
    UNW_AARCH64_X9 = 9,
    UNW_AARCH64_X10 = 10,
    UNW_AARCH64_X11 = 11,
    UNW_AARCH64_X12 = 12,
    UNW_AARCH64_X13 = 13,
    UNW_AARCH64_X14 = 14,
    UNW_AARCH64_X15 = 15,
    UNW_AARCH64_X16 = 16,
    UNW_AARCH64_X17 = 17,
    UNW_AARCH64_X18 = 18,
    UNW_AARCH64_X19 = 19,
    UNW_AARCH64_X20 = 20,
    UNW_AARCH64_X21 = 21,
    UNW_AARCH64_X22 = 22,
    UNW_AARCH64_X23 = 23,
    UNW_AARCH64_X24 = 24,
    UNW_AARCH64_X25 = 25,
    UNW_AARCH64_X26 = 26,
    UNW_AARCH64_X27 = 27,
    UNW_AARCH64_X28 = 28,
    UNW_AARCH64_X29 = 29,
    UNW_AARCH64_FP = 29,
    UNW_AARCH64_X30 = 30,
    UNW_AARCH64_LR = 30,
    UNW_AARCH64_X31 = 31,
    UNW_AARCH64_SP = 31,
    UNW_AARCH64_PC = 32,
    UNW_AARCH64_VG = 46,

    // reserved block
    UNW_AARCH64_RA_SIGN_STATE = 34,

    // FP/vector registers
    UNW_AARCH64_V0 = 64,
    UNW_AARCH64_V1 = 65,
    UNW_AARCH64_V2 = 66,
    UNW_AARCH64_V3 = 67,
    UNW_AARCH64_V4 = 68,
    UNW_AARCH64_V5 = 69,
    UNW_AARCH64_V6 = 70,
    UNW_AARCH64_V7 = 71,
    UNW_AARCH64_V8 = 72,
    UNW_AARCH64_V9 = 73,
    UNW_AARCH64_V10 = 74,
    UNW_AARCH64_V11 = 75,
    UNW_AARCH64_V12 = 76,
    UNW_AARCH64_V13 = 77,
    UNW_AARCH64_V14 = 78,
    UNW_AARCH64_V15 = 79,
    UNW_AARCH64_V16 = 80,
    UNW_AARCH64_V17 = 81,
    UNW_AARCH64_V18 = 82,
    UNW_AARCH64_V19 = 83,
    UNW_AARCH64_V20 = 84,
    UNW_AARCH64_V21 = 85,
    UNW_AARCH64_V22 = 86,
    UNW_AARCH64_V23 = 87,
    UNW_AARCH64_V24 = 88,
    UNW_AARCH64_V25 = 89,
    UNW_AARCH64_V26 = 90,
    UNW_AARCH64_V27 = 91,
    UNW_AARCH64_V28 = 92,
    UNW_AARCH64_V29 = 93,
    UNW_AARCH64_V30 = 94,
    UNW_AARCH64_V31 = 95,

    // Compatibility aliases
    UNW_ARM64_X0 = UNW_AARCH64_X0,
    UNW_ARM64_X1 = UNW_AARCH64_X1,
    UNW_ARM64_X2 = UNW_AARCH64_X2,
    UNW_ARM64_X3 = UNW_AARCH64_X3,
    UNW_ARM64_X4 = UNW_AARCH64_X4,
    UNW_ARM64_X5 = UNW_AARCH64_X5,
    UNW_ARM64_X6 = UNW_AARCH64_X6,
    UNW_ARM64_X7 = UNW_AARCH64_X7,
    UNW_ARM64_X8 = UNW_AARCH64_X8,
    UNW_ARM64_X9 = UNW_AARCH64_X9,
    UNW_ARM64_X10 = UNW_AARCH64_X10,
    UNW_ARM64_X11 = UNW_AARCH64_X11,
    UNW_ARM64_X12 = UNW_AARCH64_X12,
    UNW_ARM64_X13 = UNW_AARCH64_X13,
    UNW_ARM64_X14 = UNW_AARCH64_X14,
    UNW_ARM64_X15 = UNW_AARCH64_X15,
    UNW_ARM64_X16 = UNW_AARCH64_X16,
    UNW_ARM64_X17 = UNW_AARCH64_X17,
    UNW_ARM64_X18 = UNW_AARCH64_X18,
    UNW_ARM64_X19 = UNW_AARCH64_X19,
    UNW_ARM64_X20 = UNW_AARCH64_X20,
    UNW_ARM64_X21 = UNW_AARCH64_X21,
    UNW_ARM64_X22 = UNW_AARCH64_X22,
    UNW_ARM64_X23 = UNW_AARCH64_X23,
    UNW_ARM64_X24 = UNW_AARCH64_X24,
    UNW_ARM64_X25 = UNW_AARCH64_X25,
    UNW_ARM64_X26 = UNW_AARCH64_X26,
    UNW_ARM64_X27 = UNW_AARCH64_X27,
    UNW_ARM64_X28 = UNW_AARCH64_X28,
    UNW_ARM64_X29 = UNW_AARCH64_X29,
    UNW_ARM64_FP = UNW_AARCH64_FP,
    UNW_ARM64_X30 = UNW_AARCH64_X30,
    UNW_ARM64_LR = UNW_AARCH64_LR,
    UNW_ARM64_X31 = UNW_AARCH64_X31,
    UNW_ARM64_SP = UNW_AARCH64_SP,
    UNW_ARM64_PC = UNW_AARCH64_PC,
    UNW_ARM64_RA_SIGN_STATE = UNW_AARCH64_RA_SIGN_STATE,
    UNW_ARM64_D0 = UNW_AARCH64_V0,
    UNW_ARM64_D1 = UNW_AARCH64_V1,
    UNW_ARM64_D2 = UNW_AARCH64_V2,
    UNW_ARM64_D3 = UNW_AARCH64_V3,
    UNW_ARM64_D4 = UNW_AARCH64_V4,
    UNW_ARM64_D5 = UNW_AARCH64_V5,
    UNW_ARM64_D6 = UNW_AARCH64_V6,
    UNW_ARM64_D7 = UNW_AARCH64_V7,
    UNW_ARM64_D8 = UNW_AARCH64_V8,
    UNW_ARM64_D9 = UNW_AARCH64_V9,
    UNW_ARM64_D10 = UNW_AARCH64_V10,
    UNW_ARM64_D11 = UNW_AARCH64_V11,
    UNW_ARM64_D12 = UNW_AARCH64_V12,
    UNW_ARM64_D13 = UNW_AARCH64_V13,
    UNW_ARM64_D14 = UNW_AARCH64_V14,
    UNW_ARM64_D15 = UNW_AARCH64_V15,
    UNW_ARM64_D16 = UNW_AARCH64_V16,
    UNW_ARM64_D17 = UNW_AARCH64_V17,
    UNW_ARM64_D18 = UNW_AARCH64_V18,
    UNW_ARM64_D19 = UNW_AARCH64_V19,
    UNW_ARM64_D20 = UNW_AARCH64_V20,
    UNW_ARM64_D21 = UNW_AARCH64_V21,
    UNW_ARM64_D22 = UNW_AARCH64_V22,
    UNW_ARM64_D23 = UNW_AARCH64_V23,
    UNW_ARM64_D24 = UNW_AARCH64_V24,
    UNW_ARM64_D25 = UNW_AARCH64_V25,
    UNW_ARM64_D26 = UNW_AARCH64_V26,
    UNW_ARM64_D27 = UNW_AARCH64_V27,
    UNW_ARM64_D28 = UNW_AARCH64_V28,
    UNW_ARM64_D29 = UNW_AARCH64_V29,
    UNW_ARM64_D30 = UNW_AARCH64_V30,
    UNW_ARM64_D31 = UNW_AARCH64_V31,
};

#pragma pack(push,1)
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
};
static_assert(sizeof(cie_t)==0x1c+4);
static_assert(sizeof(cie_t)%4==0);
struct fde_t{
    uint32_t len;
    uint32_t cie_pointer;
    uint64_t pc_start;
    uint64_t pc_range;
    uint8_t augmentation_data_size;
    uint8_t augmentation_data[4];
    uint8_t program[203];
};

//static_assert(sizeof(fde_t)==0xe4+4);
static_assert(sizeof(fde_t)%4==0);
struct eh_frame_t{
    cie_t cie;
    fde_t fde;
};
#pragma pack(pop)
#if 0
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
static int encode_uleb128(uint8_t* out,uint32_t in){
    uint8_t* p=out;
    int size=0;
    do{
        uint8_t byte=in&0x7f;
        in>>=7;
        if(in!=0) byte|=0x80;
        *p++=byte;
        size++;
    }while(in!=0);
    return size;
}

namespace xe {
    namespace cpu {
        namespace backend {
            namespace a64 {

                class PosixA64CodeCache : public A64CodeCache {
                    std::vector<eh_frame_t> eh_frame_list_;
                    eh_frame_t eh_frame_template_;
                public:
                    PosixA64CodeCache();
                    ~PosixA64CodeCache() override;

                    bool Initialize() override;

                    void* LookupUnwindInfo(uint64_t host_pc) override;

                private:
                    void PlaceCode(uint32_t guest_address, void* machine_code,
                                   const EmitFunctionInfo& func_info,
                                   void* code_execute_address,
                                   UnwindReservation unwind_reservation) override;

                    void InitEhFrameTemplate();
                    void RegisterFDE(void* code_execute_address,const EmitFunctionInfo& func_info);
                    void UpdateFDE_Program(fde_t& fde,const EmitFunctionInfo& func_info);

                };

                std::unique_ptr<A64CodeCache> A64CodeCache::Create() {
                    return std::make_unique<PosixA64CodeCache>();
                }

                PosixA64CodeCache::PosixA64CodeCache() = default;
                PosixA64CodeCache::~PosixA64CodeCache() {
                    for(eh_frame_t& frame:eh_frame_list_){
                        __deregister_frame(&frame.fde);
                    }
                    eh_frame_list_.clear();
                }

                bool PosixA64CodeCache::Initialize() {
                    if (!A64CodeCache::Initialize()) {
                        return false;
                    }
                    InitEhFrameTemplate();
                    eh_frame_list_.reserve(kMaximumFunctionCount);
                    return true;
                }

                void PosixA64CodeCache::InitEhFrameTemplate(){
                    //CIE
                    eh_frame_template_.cie.len=sizeof(cie_t)-4;
                    eh_frame_template_.cie.id=0;
                    eh_frame_template_.cie.version=1;
                    memcpy(eh_frame_template_.cie.augmentation,"zPLR\0",5);
                    eh_frame_template_.cie.code_alignment_factor=1;//ULEB128 1
                    eh_frame_template_.cie.data_alignment_factor=0x78;//SLEB128 -8
                    eh_frame_template_.cie.return_address_register=30;
                    eh_frame_template_.cie.augmentation_data_size=11;
                    eh_frame_template_.cie.augmentation_data[0+0]=DW_EH_PE_absptr|DW_EH_PE_udata8;
                    {
                        //uint64_t p__jit_personality=reinterpret_cast<uint64_t>(__jit_personality);
                        uint64_t p__jit_personality=reinterpret_cast<uint64_t>(__gxx_personality_v0);
                        memcpy(&eh_frame_template_.cie.augmentation_data[0+1],&p__jit_personality,8);
                    }
                    eh_frame_template_.cie.augmentation_data[1+8]=DW_EH_PE_pcrel|DW_EH_PE_sdata4;
                    eh_frame_template_.cie.augmentation_data[2+8]=DW_EH_PE_absptr|DW_EH_PE_udata8;
                    uint8_t cie_program[3]={DW_CFA_def_cfa,31,0};//DW_CFA_def_cfa: reg31(SP) +0
                    memcpy(eh_frame_template_.cie.program,cie_program,3);

                    //FDE
                    eh_frame_template_.fde.len=sizeof(fde_t)-4;
                    eh_frame_template_.fde.cie_pointer=4+eh_frame_template_.cie.len+4;
                    eh_frame_template_.fde.augmentation_data_size=4;
                    memset(eh_frame_template_.fde.augmentation_data,0,4);//LSDA
                }
                void PosixA64CodeCache::RegisterFDE(void* code_execute_address,const EmitFunctionInfo& func_info){
                    eh_frame_list_.push_back({});
                    eh_frame_t& frame=eh_frame_list_.back();
                    memcpy(&frame,&eh_frame_template_,sizeof(eh_frame_t));

                    frame.fde.pc_start=reinterpret_cast<uint64_t>(code_execute_address);
                    frame.fde.pc_range=func_info.code_size.total;

                    UpdateFDE_Program(frame.fde,func_info);
                    __register_frame(&frame.fde);
                }

                void PosixA64CodeCache::UpdateFDE_Program(fde_t& fde,const EmitFunctionInfo &func_info) {
                    /*
                     * stp x29, x30, [sp, #-0x10]!
                     * mov x29, sp
                     * sub sp, sp, stack_size
                     * ...函数体...
                     * add sp, sp, stack_size
                     * mov sp, x29
                     * ldp x29, x30, [sp], #0x10
                     * ret
                     */

                    uint8_t default_fde_program[32]={
                            DW_CFA_def_cfa,31,0,

                            DW_CFA_advance_loc|4 ,
                            DW_CFA_def_cfa_offset ,16,
                            DW_CFA_offset|29,2,
                            DW_CFA_offset|30,1,

                            DW_CFA_advance_loc|4 ,
                            DW_CFA_def_cfa_register,29,

                            DW_CFA_advance_loc|4,
                            DW_CFA_nop,

                            DW_CFA_advance_loc4 ,0,0,0,0,//

                            DW_CFA_advance_loc|4,
                            DW_CFA_nop,

                            DW_CFA_advance_loc|4 ,
                            DW_CFA_def_cfa_register,31,

                            DW_CFA_advance_loc|4,
                            DW_CFA_def_cfa_offset,0,
                            DW_CFA_restore|29,
                            DW_CFA_restore|30,

                            DW_CFA_advance_loc|4,
                            DW_CFA_nop,
                    };

                    // HostToGuestThunk
                    /*
                     * sub sp, sp, stack_size // stack_size=0x200,简化计算
                     * stp x19, x20, [sp, #0x18]
                     * stp x21, x22, [sp, #0x28]
                     * stp x23, x24, [sp, #0x38]
                     * stp x25, x26, [sp, #0x48]
                     * stp x27, x28, [sp, #0x58]
                     * stp x29, x30, [sp, #0x68]
                     * str x17, [sp, #0x78]
                     * stp d8, d9, [sp, #0xa0]
                     * stp d10, d11, [sp, #0xb0]
                     * stp d12, d13, [sp, #0xc0]
                     * stp d14, d15, [sp, #0xd0]
                     * ...函数体...
                     * ldp x19, x20, [sp, #0x18]
                     * ldp x21, x22, [sp, #0x28]
                     * ldp x23, x24, [sp, #0x38]
                     * ldp x25, x26, [sp, #0x48]
                     * ldp x27, x28, [sp, #0x58]
                     * ldp x29, x30, [sp, #0x68]
                     * ldr x17, [sp, #0x78]
                     * ldp d8, d9, [sp, #0xa0]
                     * ldp d10, d11, [sp, #0xb0]
                     * ldp d12, d13, [sp, #0xc0]
                     * ldp d14, d15, [sp, #0xd0]
                     * add sp, sp, stack_size // stack_size=0x200,简化计算
                     * ret
                     */

                    uint8_t htg_thunk_fde_program[]={
                            DW_CFA_def_cfa,31,0,

                            DW_CFA_advance_loc|4 ,
                            DW_CFA_def_cfa_offset ,0x80,0x04, //0x200

                            DW_CFA_advance_loc | 44,

                            DW_CFA_offset | 19, 61,// (0x200-0x18)/8
                            DW_CFA_offset | 20, 60,
                            DW_CFA_offset | 21, 59,
                            DW_CFA_offset | 22, 58,
                            DW_CFA_offset | 23, 57,
                            DW_CFA_offset | 24, 56,
                            DW_CFA_offset | 25, 55,
                            DW_CFA_offset | 26, 54,
                            DW_CFA_offset | 27, 53,
                            DW_CFA_offset | 28, 52,
                            DW_CFA_offset | 29, 51,
                            DW_CFA_offset | 30, 50,
                            DW_CFA_offset | 17, 49,

                            DW_CFA_offset_extended ,(64+8), 44,
                            DW_CFA_offset_extended , (64+9), 43,
                            DW_CFA_offset_extended , (64+10), 42,
                            DW_CFA_offset_extended , (64+11), 41,
                            DW_CFA_offset_extended , (64+12), 40,
                            DW_CFA_offset_extended , (64+13), 39,
                            DW_CFA_offset_extended , (64+14), 38,
                            DW_CFA_offset_extended , (64+15), 37,

                            DW_CFA_advance_loc4 ,0,0,0,0,//

                            DW_CFA_advance_loc | 44,

                            DW_CFA_restore | 19,
                            DW_CFA_restore | 20,
                            DW_CFA_restore | 21,
                            DW_CFA_restore | 22,
                            DW_CFA_restore | 23,
                            DW_CFA_restore | 24,
                            DW_CFA_restore | 25,
                            DW_CFA_restore | 26,
                            DW_CFA_restore | 27,
                            DW_CFA_restore | 28,
                            DW_CFA_restore | 29,
                            DW_CFA_restore | 30,
                            DW_CFA_restore | 17,

                            DW_CFA_restore_extended,64+8,
                            DW_CFA_restore_extended,64+9,
                            DW_CFA_restore_extended,64+10,
                            DW_CFA_restore_extended,64+11,
                            DW_CFA_restore_extended,64+12,
                            DW_CFA_restore_extended,64+13,
                            DW_CFA_restore_extended,64+14,
                            DW_CFA_restore_extended,64+15,

                            DW_CFA_advance_loc|4,
                            DW_CFA_def_cfa_offset,0,

                            DW_CFA_advance_loc|4,
                            DW_CFA_nop,
                    };
                    //GuestToHostThunk
                    /*
                     * sub sp, sp, stack_size // stack_size=0x200，简化计算
                     * stp x1, x2, [sp, #0x18]
                     * stp x3, x4, [sp, #0x28]
                     * stp x5, x6, [sp, #0x38]
                     * stp x7, x8, [sp, #0x48]
                     * stp x9, x10, [sp, #0x58]
                     * stp x11, x12, [sp, #0x68]
                     * stp x13, x14, [sp, #0x78]
                     * stp x15, x30, [sp, #0x88]
                     * stp q1, q2, [sp, #0xa0]
                     * stp q3, q4, [sp, #0xc0]
                     * stp q5, q6, [sp, #0xe0]
                     * stp q7, q16, [sp, #0x100]
                     * stp q17, q18, [sp, #0x120]
                     * stp q19, q20, [sp, #0x140]
                     * stp q21, q22, [sp, #0x160]
                     * stp q23, q24, [sp, #0x180]
                     * stp q25, q26, [sp, #0x1a0]
                     * stp q27, q28, [sp, #0x1c0]
                     * stp q29, q30, [sp, #0x1e0]
                     * str q31, [sp, #0x1f0]
                     * ...函数体
                     * ldp x1, x2, [sp, #0x18]
                     * ldp x3, x4, [sp, #0x28]
                     * ldp x5, x6, [sp, #0x38]
                     * ldp x7, x8, [sp, #0x48]
                     * ldp x9, x10, [sp, #0x58]
                     * ldp x11, x12, [sp, #0x68]
                     * ldp x13, x14, [sp, #0x78]
                     * ldp x15, x30, [sp, #0x88]
                     * ldp q1, q2, [sp, #0xa0]
                     * ldp q3, q4, [sp, #0xc0]
                     * ldp q5, q6, [sp, #0xe0]
                     * ldp q7, q16, [sp, #0x100]
                     * ldp q17, q18, [sp, #0x120]
                     * ldp q19, q20, [sp, #0x140]
                     * ldp q21, q22, [sp, #0x160]
                     * ldp q21, q22, [sp, #0x160]
                     * ldp q23, q24, [sp, #0x180]
                     * ldp q25, q26, [sp, #0x1a0]
                     * ldp q27, q28, [sp, #0x1c0]
                     * ldp q29, q30, [sp, #0x1e0]
                     * ldr q31, [sp, #0x1f0]
                     * add sp, sp, stack_size // stack_size=0x200，简化计算
                     * ret
                     * */
                    uint8_t gth_thunk_fde_program[]={
                            DW_CFA_def_cfa,31,0,

                            DW_CFA_advance_loc|4,
                            DW_CFA_def_cfa_offset,0x80,0x04, //0x200

                            DW_CFA_advance_loc1,80,

                            DW_CFA_offset|1,61,   //(0x200-0x18)/8
                            DW_CFA_offset|2,60,
                            DW_CFA_offset|3,59,
                            DW_CFA_offset|4,58,
                            DW_CFA_offset|5,57,
                            DW_CFA_offset|6,56,
                            DW_CFA_offset|7,55,
                            DW_CFA_offset|8,54,
                            DW_CFA_offset|9,53,
                            DW_CFA_offset|10,52,
                            DW_CFA_offset|11,51,
                            DW_CFA_offset|12,50,
                            DW_CFA_offset|13,49,
                            DW_CFA_offset|14,48,
                            DW_CFA_offset|15,47,
                            DW_CFA_offset|30,46,

                            DW_CFA_offset_extended,(64+1),44,
                            DW_CFA_offset_extended,(64+2),42,
                            DW_CFA_offset_extended,(64+3),40,
                            DW_CFA_offset_extended,(64+4),38,
                            DW_CFA_offset_extended,(64+5),36,
                            DW_CFA_offset_extended,(64+6),34,
                            DW_CFA_offset_extended,(64+7),32,
                            DW_CFA_offset_extended,(64+16),30,
                            DW_CFA_offset_extended,(64+17),28,
                            DW_CFA_offset_extended,(64+18),26,
                            DW_CFA_offset_extended,(64+19),24,
                            DW_CFA_offset_extended,(64+20),22,
                            DW_CFA_offset_extended,(64+21),20,
                            DW_CFA_offset_extended,(64+22),18,
                            DW_CFA_offset_extended,(64+23),16,
                            DW_CFA_offset_extended,(64+24),14,
                            DW_CFA_offset_extended,(64+25),12,
                            DW_CFA_offset_extended,(64+26),10,
                            DW_CFA_offset_extended,(64+27),8,
                            DW_CFA_offset_extended,(64+28),6,
                            DW_CFA_offset_extended,(64+29),4,
                            DW_CFA_offset_extended,(64+30),2,
                            DW_CFA_offset_extended,(64+31),0,

                            DW_CFA_advance_loc4,0,0,0,0,//

                            DW_CFA_advance_loc1,80,

                            DW_CFA_restore|1,
                            DW_CFA_restore|2,
                            DW_CFA_restore|3,
                            DW_CFA_restore|4,
                            DW_CFA_restore|5,
                            DW_CFA_restore|6,
                            DW_CFA_restore|7,
                            DW_CFA_restore|8,
                            DW_CFA_restore|9,
                            DW_CFA_restore|10,
                            DW_CFA_restore|11,
                            DW_CFA_restore|12,
                            DW_CFA_restore|13,
                            DW_CFA_restore|14,
                            DW_CFA_restore|15,
                            DW_CFA_restore|30,

                            DW_CFA_restore_extended,(64+1),
                            DW_CFA_restore_extended,(64+2),
                            DW_CFA_restore_extended,(64+3),
                            DW_CFA_restore_extended,(64+4),
                            DW_CFA_restore_extended,(64+5),
                            DW_CFA_restore_extended,(64+6),
                            DW_CFA_restore_extended,(64+7),
                            DW_CFA_restore_extended,(64+16),
                            DW_CFA_restore_extended,(64+17),
                            DW_CFA_restore_extended,(64+18),
                            DW_CFA_restore_extended,(64+19),
                            DW_CFA_restore_extended,(64+20),
                            DW_CFA_restore_extended,(64+21),
                            DW_CFA_restore_extended,(64+22),
                            DW_CFA_restore_extended,(64+23),
                            DW_CFA_restore_extended,(64+24),
                            DW_CFA_restore_extended,(64+25),
                            DW_CFA_restore_extended,(64+26),
                            DW_CFA_restore_extended,(64+27),
                            DW_CFA_restore_extended,(64+28),
                            DW_CFA_restore_extended,(64+29),
                            DW_CFA_restore_extended,(64+30),
                            DW_CFA_restore_extended,(64+31),

                            DW_CFA_advance_loc|4,
                            DW_CFA_def_cfa_offset,0,

                            DW_CFA_advance_loc|4,
                            DW_CFA_nop,
                    };
                    //ResolveFunction
                    /*
                     * stp xzr,x0,[sp, #-0x10]!
                     * sub sp, sp, stack_size // stack_size=0x200,简化计算
                     * stp x1, x2, [sp, #0x18]
                     * stp x3, x4, [sp, #0x28]
                     * stp x5, x6, [sp, #0x38]
                     * stp x7, x8, [sp, #0x48]
                     * stp x9, x10, [sp, #0x58]
                     * stp x11, x12, [sp, #0x68]
                     * stp x13, x14, [sp, #0x78]
                     * stp x15, x30, [sp, #0x88]
                     * stp q1, q2, [sp, #0xa0]
                     * stp q3, q4, [sp, #0xc0]
                     * stp q5, q6, [sp, #0xe0]
                     * stp q7, q16, [sp, #0x100]
                     * stp q17, q18, [sp, #0x120]
                     * stp q19, q20, [sp, #0x140]
                     * stp q21, q22, [sp, #0x160]
                     * stp q23, q24, [sp, #0x180]
                     * stp q25, q26, [sp, #0x1a0]
                     * stp q27, q28, [sp, #0x1c0]
                     * stp q29, q30, [sp, #0x1e0]
                     * str q31, [sp, #0x1f0]
                     * ...函数体...
                     * ldp x1, x2, [sp, #0x18]
                     * ldp x3, x4, [sp, #0x28]
                     * ldp x5, x6, [sp, #0x38]
                     * ldp x7, x8, [sp, #0x48]
                     * ldp x9, x10, [sp, #0x58]
                     * ldp x11, x12, [sp, #0x68]
                     * ldp x13, x14, [sp, #0x78]
                     * ldp x15, x30, [sp, #0x88]
                     * ldp q1, q2, [sp, #0xa0]
                     * ldp q3, q4, [sp, #0xc0]
                     * ldp q5, q6, [sp, #0xe0]
                     * ldp q7, q16, [sp, #0x100]
                     * ldp q17, q18, [sp, #0x120]
                     * ldp q19, q20, [sp, #0x140]
                     * ldp q21, q22, [sp, #0x160]
                     * ldp q21, q22, [sp, #0x160]
                     * ldp q23, q24, [sp, #0x180]
                     * ldp q25, q26, [sp, #0x1a0]
                     * ldp q27, q28, [sp, #0x1c0]
                     * ldp q29, q30, [sp, #0x1e0]
                     * ldr q31, [sp, #0x1f0]
                     * add sp, sp, stack_size // stack_size=0x200,简化计算
                     * ldp xzr,x0,[sp], #0x10
                     * br x16
                     */
                    uint8_t rf_thunk_fde_program[]={
                            DW_CFA_def_cfa,31,0,

                            DW_CFA_advance_loc|4,
                            DW_CFA_def_cfa_offset,16,
                            DW_CFA_offset|0,1,

                            DW_CFA_advance_loc|4,
                            DW_CFA_def_cfa_offset,0x90,0x04, //0x200+0x10

                            DW_CFA_advance_loc1,80,

                            DW_CFA_offset|1,63,   //(0x200+0x10-0x18)/8
                            DW_CFA_offset|2,62,
                            DW_CFA_offset|3,61,
                            DW_CFA_offset|4,60,
                            DW_CFA_offset|5,59,
                            DW_CFA_offset|6,58,
                            DW_CFA_offset|7,57,
                            DW_CFA_offset|8,56,
                            DW_CFA_offset|9,55,
                            DW_CFA_offset|10,54,
                            DW_CFA_offset|11,53,
                            DW_CFA_offset|12,52,
                            DW_CFA_offset|13,51,
                            DW_CFA_offset|14,50,
                            DW_CFA_offset|15,49,
                            DW_CFA_offset|30,48,

                            DW_CFA_offset_extended,(64+1),46,
                            DW_CFA_offset_extended,(64+2),44,
                            DW_CFA_offset_extended,(64+3),42,
                            DW_CFA_offset_extended,(64+4),40,
                            DW_CFA_offset_extended,(64+5),38,
                            DW_CFA_offset_extended,(64+6),36,
                            DW_CFA_offset_extended,(64+7),34,
                            DW_CFA_offset_extended,(64+16),32,
                            DW_CFA_offset_extended,(64+17),30,
                            DW_CFA_offset_extended,(64+18),28,
                            DW_CFA_offset_extended,(64+19),26,
                            DW_CFA_offset_extended,(64+20),24,
                            DW_CFA_offset_extended,(64+21),22,
                            DW_CFA_offset_extended,(64+22),20,
                            DW_CFA_offset_extended,(64+23),18,
                            DW_CFA_offset_extended,(64+24),16,
                            DW_CFA_offset_extended,(64+25),14,
                            DW_CFA_offset_extended,(64+26),12,
                            DW_CFA_offset_extended,(64+27),10,
                            DW_CFA_offset_extended,(64+28),8,
                            DW_CFA_offset_extended,(64+29),6,
                            DW_CFA_offset_extended,(64+30),4,
                            DW_CFA_offset_extended,(64+31),2,

                            DW_CFA_advance_loc4,0,0,0,0,//

                            DW_CFA_advance_loc1,80,

                            DW_CFA_restore|1,
                            DW_CFA_restore|2,
                            DW_CFA_restore|3,
                            DW_CFA_restore|4,
                            DW_CFA_restore|5,
                            DW_CFA_restore|6,
                            DW_CFA_restore|7,
                            DW_CFA_restore|8,
                            DW_CFA_restore|9,
                            DW_CFA_restore|10,
                            DW_CFA_restore|11,
                            DW_CFA_restore|12,
                            DW_CFA_restore|13,
                            DW_CFA_restore|14,
                            DW_CFA_restore|15,
                            DW_CFA_restore|30,

                            DW_CFA_restore_extended,(64+1),
                            DW_CFA_restore_extended,(64+2),
                            DW_CFA_restore_extended,(64+3),
                            DW_CFA_restore_extended,(64+4),
                            DW_CFA_restore_extended,(64+5),
                            DW_CFA_restore_extended,(64+6),
                            DW_CFA_restore_extended,(64+7),
                            DW_CFA_restore_extended,(64+16),
                            DW_CFA_restore_extended,(64+17),
                            DW_CFA_restore_extended,(64+18),
                            DW_CFA_restore_extended,(64+19),
                            DW_CFA_restore_extended,(64+20),
                            DW_CFA_restore_extended,(64+21),
                            DW_CFA_restore_extended,(64+22),
                            DW_CFA_restore_extended,(64+23),
                            DW_CFA_restore_extended,(64+24),
                            DW_CFA_restore_extended,(64+25),
                            DW_CFA_restore_extended,(64+26),
                            DW_CFA_restore_extended,(64+27),
                            DW_CFA_restore_extended,(64+28),
                            DW_CFA_restore_extended,(64+29),
                            DW_CFA_restore_extended,(64+30),
                            DW_CFA_restore_extended,(64+31),

                            DW_CFA_advance_loc|4,
                            DW_CFA_def_cfa_offset,16,

                            DW_CFA_advance_loc|4,
                            DW_CFA_def_cfa_offset,0,
                            DW_CFA_restore|0,

                            DW_CFA_advance_loc|4,
                            DW_CFA_nop,
                    };

                    uint8_t nop_fde_program[]={
                            DW_CFA_def_cfa,31,0,
                            DW_CFA_advance_loc4 ,0,0,0,0,//,
                    };
                    memset(fde.program,0,sizeof(fde.program));

                    if(func_info.code_size.prolog==12*4){

                        static_assert(sizeof(htg_thunk_fde_program)<=sizeof(fde.program));
                        assert(func_info.stack_size==0x200);
                        memcpy(fde.program,htg_thunk_fde_program,sizeof(htg_thunk_fde_program));

                        uint8_t body_size[4];
                        memcpy(body_size,&func_info.code_size.body,4);
                        memcpy(fde.program+59,body_size,4);

                    }
                    else if(func_info.code_size.prolog==21*4){
                        static_assert(sizeof(gth_thunk_fde_program)<=sizeof(fde.program));
                        assert(func_info.stack_size==0x200);
                        memcpy(fde.program,gth_thunk_fde_program,sizeof(gth_thunk_fde_program));

                        uint8_t body_size[4];
                        memcpy(body_size,&func_info.code_size.body,4);
                        memcpy(fde.program+111,body_size,4);
                    }
                    else if(func_info.code_size.prolog==22*4){

                        static_assert(sizeof(rf_thunk_fde_program)<=sizeof(fde.program));
                        assert(func_info.stack_size==0x200);
                        memcpy(fde.program,rf_thunk_fde_program,sizeof(rf_thunk_fde_program));

                        uint8_t body_size[4];
                        memcpy(body_size,&func_info.code_size.body,4);
                        memcpy(fde.program+116,body_size,4);
                    }

                    else if(func_info.code_size.prolog==3*4){
                        static_assert(sizeof(default_fde_program)<=sizeof(fde.program));
                        memcpy(fde.program,default_fde_program,sizeof(default_fde_program));

                        uint8_t body_size[4];
                        memcpy(body_size,&func_info.code_size.body,4);
                        memcpy(fde.program+16,body_size,4);
                    }
                    else{
                        XELOGI("assert_always");
                        assert_always("--");
                    }
                }

                static void dis_asm(void* machine_code, const EmitFunctionInfo& func_info,
                                    void* code_execute_address) {
                    uint32_t* ec = reinterpret_cast<uint32_t*>(code_execute_address);
                    size_t offset = 0;

                    XELOGI("asm: {}:",func_info.stack_size);
                    // prolog
                    if (func_info.code_size.prolog > 0) {
                        XELOGI("prolog:");
                        size_t prolog_count = func_info.code_size.prolog / 4;

                        XELOGI("prolog({}):",prolog_count);
                        std::string prolog_asm = aarch64_disasm(reinterpret_cast<uint64_t>(code_execute_address), ec, prolog_count);
                        XELOGI(prolog_asm);
                        offset += prolog_count;
                    }

                    // body
                    if (func_info.code_size.body > 0) {
                        size_t body_count = func_info.code_size.body / 4;

                        XELOGI("body({}):",body_count);
                        std::string body_asm = aarch64_disasm(reinterpret_cast<uint64_t>(code_execute_address) + offset * 4, ec + offset, body_count);
                        XELOGI(body_asm);
                        offset += body_count;
                    }

                    // epilog
                    if (func_info.code_size.epilog > 0) {
                        size_t epilog_count = func_info.code_size.epilog / 4;

                        XELOGI("epilog({}):",epilog_count);
                        std::string epilog_asm = aarch64_disasm(reinterpret_cast<uint64_t>(code_execute_address) + offset * 4, ec + offset, epilog_count);
                        XELOGI(epilog_asm);
                        offset += epilog_count;
                    }
                }

                void PosixA64CodeCache::PlaceCode(uint32_t guest_address, void* machine_code,
                                                  const EmitFunctionInfo& func_info,
                                                  void* code_execute_address,
                                                  UnwindReservation unwind_reservation){

                    RegisterFDE(code_execute_address,func_info);
                    assert_false(eh_frame_list_.size() >= kMaximumFunctionCount);

                    uint32_t* ec=reinterpret_cast<uint32_t*>(code_execute_address);
                    //XELOGI("ASM:\n{}", aarch64_disasm(reinterpret_cast<uint64_t>(code_execute_address),ec,func_info.code_size.total/4));

                    //dis_asm(machine_code, func_info, code_execute_address);

                    __clear_cache(code_execute_address, reinterpret_cast<uint8_t*>(code_execute_address)+func_info.code_size.total);
                }

                void* PosixA64CodeCache::LookupUnwindInfo(uint64_t host_pc) {
                    return nullptr;
                }

            }
        }
    }
}