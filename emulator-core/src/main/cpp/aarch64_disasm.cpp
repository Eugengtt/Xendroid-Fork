// SPDX-License-Identifier: WTFPL
#include "aarch64_disasm.h"
#include <sstream>
#include "third_party/capstone/include/capstone/arm64.h"
#include "third_party/capstone/include/capstone/capstone.h"
std::string aarch64_disasm(uint64_t base, uint32_t* code,size_t count){
    csh capstone_handle_;
    if (cs_open(CS_ARCH_AARCH64, CS_MODE_LITTLE_ENDIAN, &capstone_handle_) !=
        CS_ERR_OK) {
        return "";
    }
    cs_option(capstone_handle_, CS_OPT_SYNTAX, CS_OPT_SYNTAX_INTEL);
    cs_option(capstone_handle_, CS_OPT_DETAIL, CS_OPT_OFF);
    cs_insn* instructions;
    size_t n=cs_disasm(capstone_handle_, reinterpret_cast<const uint8_t*>(code), count* sizeof(uint32_t), base, count, &instructions);
    if(n!=count){
        cs_free(instructions, n);
        return "";
    }
    std::stringstream ss;
    ss<<std::hex;
    for(size_t i=0;i<n;i++){
        ss<<instructions[i].address<<" "<<instructions[i].mnemonic<<" "<<instructions[i].op_str<<"\n";
    }
    cs_free(instructions, n);
    return ss.str();
}