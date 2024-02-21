#include "PerfUtils.h"
#include <unordered_map>
#include <cstring>
#include <string>

bool initialize_libpfm() {
    static bool _initialized = false;
    if (_initialized) return true;
#ifdef HAVE_LIBPFM
    int ret = pfm_initialize();
    if (ret != PFM_SUCCESS) {
        fprintf(stderr, "Cannot initialize library: %s", pfm_strerror(ret));
        return false;
    }
#endif
    _initialized = true;
    return true;
}

#ifdef HAVE_LIBPFM
bool translate(const char *events[], perf_event_attr_t *evds, size_t size) {
    for (size_t j = 0; j < size; ++j) {
        perf_event_attr_t &attr(evds[j]);
        // memset(&attr, 0, sizeof(attr));
        //  char *fstr = nullptr;
        pfm_perf_encode_arg_t arg;
        memset(&arg, 0, sizeof(arg));
        arg.attr = &attr;
        // arg.fstr = &fstr;
        int ret = pfm_get_os_event_encoding(events[j], PFM_PLM0 | PFM_PLM3,
                                            PFM_OS_PERF_EVENT_EXT, &arg);
        if (ret != PFM_SUCCESS) {
            std::cerr << "PerfGroup: could not translate event [" << events[j] << "] "
                      << pfm_strerror(ret) << "\n";
            return false;
        }
        // std::cerr << "Event:" << events[j] << " name: [" << fstr << "] type:" <<
        // attr.type << " size:" << attr.size << " config:" << attr.config << "\n";
        //::free(fstr);
    }
    return true;
}
#else
bool translate(const char *events[], perf_event_attr *evds, size_t size) {
    struct TypeConfig {
        int type;
        unsigned long long config;
    };
    std::unordered_map<std::string, TypeConfig> configmap = {
        {"cycles", {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES}},
        {"instructions", {PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS}},
        {"cache-misses", {PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_MISSES}},
        {"branch-misses", {PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES}}};
    for (size_t j = 0; j < size; ++j) {
        perf_event_attr &pe(evds[j]);
        memset(&pe, 0, sizeof(pe));
        auto it = configmap.find(events[j]);
        if (it != configmap.end()) {
            pe.type = it->second.type;
            pe.config = it->second.config;
        }
    }
    return true;
}
#endif
