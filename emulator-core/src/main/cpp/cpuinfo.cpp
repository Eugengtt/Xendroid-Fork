
// SPDX-License-Identifier: WTFPL
// Created by xendroid on 2025/5/31.
//
#include "cpuinfo.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <android/log.h>

#define LOG_TAG "cpuinfo"

#define LOGW(...) {      \
    __android_log_print(ANDROID_LOG_WARN, LOG_TAG,"%s : %d",__func__,__LINE__);\
	__android_log_print(ANDROID_LOG_WARN, LOG_TAG,__VA_ARGS__);\
}

std::vector<core_info_t> cpu_get_core_info(){
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (!cpuinfo.is_open()) {
        return {};
    }

    std::vector<core_info_t> cores;
    core_info_t core;

    std::string line;
    while (std::getline(cpuinfo, line)) {

        if (line.find("processor") != std::string::npos) {
            core.processor = std::stoi(line.substr(line.find(":") + 2));
        }
        else if (line.find("CPU implementer") != std::string::npos) {
            core.implementer = std::stoi(line.substr(line.find(":") + 2), nullptr, 16);
        }
        else if (line.find("CPU variant") != std::string::npos) {
            core.variant = std::stoi(line.substr(line.find(":") + 2), nullptr, 16);
        }
        else if (line.find("CPU part") != std::string::npos) {
            core.part = std::stoi(line.substr(line.find(":") + 2), nullptr, 16);
            cores.push_back(core);
        }
        else if (line.find("Features") != std::string::npos) {
            std::string features = line.substr(line.find(":") + 2);
            std::istringstream iss(features);
            std::string feature;
            while (iss >> feature) {
                core.features.push_back(feature);
            }
        }
    }
    std::sort(cores.begin(), cores.end(), [](const core_info_t& a, const core_info_t& b) {
        return a.processor < b.processor;
    });
    return cores;
}

//map.find
bool operator<(const  core_info_t& lhs,const  core_info_t& rhs){
    if(lhs.implementer!= rhs.implementer) return lhs.implementer < rhs.implementer;
    if(lhs.part!= rhs.part) return lhs.part < rhs.part;
    return lhs.variant < rhs.variant;
}

int cpu_get_core_count(){
    return cpu_get_core_info().size();
}
int cpu_get_max_mhz(const int core_idx) {
    std::string path=std::format("/sys/devices/system/cpu/cpu{}/cpufreq/cpuinfo_max_freq", core_idx);
    std::ifstream max_freq(path);
    std::string hz;
    std::getline(max_freq, hz);
    return std::stoi(hz)/1000;
}
std::string cpu_get_simple_info(const std::vector<core_info_t>& core_info_list){
    std::map<core_info_t, int> core_counts;
    for (const auto& core : core_info_list) {
        if(core_counts.find(core) == core_counts.end())
            core_counts[core] = 1;
        else
            core_counts[core]++;
    }
    std::stringstream ss;
    for (auto it=core_counts.rbegin(),skip=--core_counts.rend(); it!=core_counts.rend();it++) {
        ss<<cpu_get_processor_name(it->first)<<"*"<<it->second;
        if(it!=skip) ss<<"+";
    }
    ss<<"("<<cpu_get_processor_isa(core_info_list[0])<<")";
    return ss.str();
}

std::set<std::string,std::greater<>> get_processor_name_set(const std::vector<core_info_t>& core_info_list){
    std::set<std::string,std::greater<>> processor_name_set;
    if(!core_info_list.empty()){
        std::for_each(core_info_list.begin(), core_info_list.end(), [&processor_name_set](const core_info_t& core_info) {
            processor_name_set.insert(cpu_get_processor_name(core_info));
        });
    }
    else{
        const std::vector<core_info_t> core_info_list2= cpu_get_core_info();
        std::for_each(core_info_list2.begin(), core_info_list2.end(), [&processor_name_set](const core_info_t& core_info) {
            processor_name_set.insert(cpu_get_processor_name(core_info));
        });
    }

    return processor_name_set;
}

struct prossessor_info_t{
    int implementer;
    int part;
    std::string name;
    std::string isa;//忽略启用的扩展
};

const std::vector<prossessor_info_t>& _get_processor_info_list(){
        static const std::vector<prossessor_info_t> processor_map{

            //ARM
            { 0x41, 0xd04, "cortex-a35", "armv8-a" },
            { 0x41, 0xd03, "cortex-a53", "armv8-a" },
            { 0x41, 0xd07, "cortex-a57", "armv8-a" },
            { 0x41, 0xd08, "cortex-a72", "armv8-a" },
            { 0x41, 0xd09, "cortex-a73", "armv8-a" },

            { 0x41, 0xd05, "cortex-a55", "armv8.2-a" },
            { 0x41, 0xd0a, "cortex-a75", "armv8.2-a" },
            { 0x41, 0xd0b, "cortex-a76", "armv8.2-a" },
            { 0x41, 0xd0d, "cortex-a77", "armv8.2-a" },
            { 0x41, 0xd41, "cortex-a78", "armv8.2-a" },
            { 0x41, 0xd44, "cortex-x1", "armv8.2-a" },

            { 0x41, 0xd46, "cortex-a510", "armv9-a" },
            { 0x41, 0xd47, "cortex-a710", "armv9-a" },
            { 0x41, 0xd48, "cortex-x2", "armv9-a" },
            { 0x41, 0xd4d, "cortex-a715", "armv9-a" },
            { 0x41, 0xd4e, "cortex-x3", "armv9-a" },

            { 0x41, 0xd80, "cortex-a520", "armv9.2-a" },
            { 0x41, 0xd81, "cortex-a720", "armv9.2-a" },
            { 0x41, 0xd87, "cortex-a725", "armv9.2-a" },
            { 0x41, 0xd82, "cortex-x4", "armv9.2-a" },
            { 0x41, 0xd85, "cortex-x925", "armv9.2-a" },

            { 0x41, 0xd88, "cortex-a520ae", "armv9.2-a" },
            //{ 0x41, 0xd06, "cortex-a65" },
            //{ 0x41, 0xd43, "cortex-a65ae" },
            { 0x41, 0xd0e, "cortex-a76ae", "armv8.2-a" },
            { 0x41, 0xd0d, "cortex-a77", "armv8.2-a" },
            { 0x41, 0xd42, "cortex-a78ae", "armv8.2-a" },
            { 0x41, 0xd4b, "cortex-a78c", "armv8.2-a" },

            //高通
            { 0x51, 0x801, "cortex-a73", "armv8-a" },// Kryo 2xx Silver
            { 0x51, 0x802, "cortex-a75", "armv8.2-a" },// Kryo 3xx Gold
            { 0x51, 0x803, "cortex-a75", "armv8.2-a" },// Kryo 3xx Silver
            { 0x51, 0x804, "cortex-a76", "armv8.2-a" },// Kryo 4xx Gold
            { 0x51, 0x805, "cortex-a76", "armv8.2-a" },// Kryo 4xx/5xx Silver
            //{{ 0x51, 0xc00}, "falkor" },
            //{{ 0x51, 0xc01}, "saphira" },
            { 0x51, 0x001, "oryon-1", "armv9.2-a" },

            //海思
            //{{ 0x48, 0xd01}, "tsv110" },

            //三星
            { 0x53, 0x002, "exynos-m3", "armv8-a" },
            { 0x53, 0x003, "exynos-m4", "armv8.2-a" },
        };
        return processor_map;
};

std::string cpu_get_processor_name(const core_info_t& core_info){


    int implementer=core_info.implementer;
    int variant=core_info.variant;
    int part=core_info.part;
    for(const prossessor_info_t& processor_info:_get_processor_info_list()){
        if(processor_info.implementer==implementer&&processor_info.part==part){
            return processor_info.name;
        }
    }

    return "Unknown";
}

std::string cpu_get_processor_name(const int core_idx){
    return cpu_get_processor_name(cpu_get_core_info()[core_idx]);
}

std::string cpu_get_processor_isa(const core_info_t& core_info){

    int implementer=core_info.implementer;
    int variant=core_info.variant;
    int part=core_info.part;
    for(const prossessor_info_t& processor_info:_get_processor_info_list()){
        if(processor_info.implementer==implementer&&processor_info.part==part){
            return processor_info.isa;
        }
    }

    return "Unknown";
}
