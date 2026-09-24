// SPDX-License-Identifier: WTFPL

#pragma once

#include <cstdint>
#include <string>
std::string aarch64_disasm(uint64_t base, uint32_t* code,size_t count);