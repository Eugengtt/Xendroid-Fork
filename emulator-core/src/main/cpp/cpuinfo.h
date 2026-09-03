
// SPDX-License-Identifier: WTFPL
// Created by xendroid on 2025/5/31.
//

#ifndef APS3E_CPUINFO_H
#define APS3E_CPUINFO_H
#include <string>
#include <vector>
#include <set>
struct core_info_t {
    int processor;
    int implementer;
    int variant;
    int part;
    std::vector<std::string> features;
};

std::vector<core_info_t> cpu_get_core_info();
int cpu_get_core_count();
int cpu_get_max_mhz(const int core_idx);
std::string cpu_get_simple_info(const std::vector<core_info_t>& core_info_list);
std::set<core_info_t> get_processor_info_set();
std::set<std::string,std::greater<>> get_processor_name_set(const std::vector<core_info_t>& core_info_list={});
std::string cpu_get_processor_name(const core_info_t& core_info);
std::string cpu_get_processor_name(const int core_idx);
std::string cpu_get_processor_isa(const core_info_t& core_info);

#endif //APS3E_CPUINFO_H
