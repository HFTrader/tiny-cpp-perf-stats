#include "PerfGroup.h"
#include <perfmon/pfmlib_perf_event.h>
#include <iostream>
#include <cstring>
#include <algorithm>

// https://sources.debian.org/src/libpfm4/4.11.1+git32-gd0b85fb-1/perf_examples/self_count.c/
// https://stackoverflow.com/questions/42088515/perf-event-open-how-to-monitoring-multiple-events
// https://github.com/wcohen/libpfm4/blob/6864dad7cf85fac9fff04bd814026e2fbc160175/perf_examples/self.c

static bool initialize() {
    static bool _initialized = false;
    if (_initialized) return true;
    int ret = pfm_initialize();
    if (ret != PFM_SUCCESS) {
        fprintf(stderr, "Cannot initialize library: %s", pfm_strerror(ret));
        return false;
    }
    _initialized = true;
    return true;
}

PerfGroup::PerfGroup() {
    initialize();
}

PerfGroup::~PerfGroup() {
    close();
}

void PerfGroup::close() {
    for (Descriptor &d : _ids) {
        ::close(d.fd);
    }
    _ids.clear();
}

static bool translate(const char *events[], perf_event_attr_t *evds, size_t size) {
    // std::cerr << "Translate: " << size << " items" << "\n";
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

static bool init(std::vector<perf_event_attr_t> &evds,
                 std::vector<PerfGroup::Descriptor> &ids, std::vector<int> &leaders) {
    pid_t pid = 0;  // getpid();
    int cpu = -1;
    int leader = -1;
    int flags = 0;
    int counter = 0;
    leaders.clear();
    for (size_t j = 0; j < evds.size(); ++j) {
        perf_event_attr_t &pea(evds[j]);
        pea.disabled = (leader < 0) ? 1 : 0;
        pea.inherit = 1;
        pea.pinned = (leader < 0) ? 1 : 0;
        pea.size = sizeof(perf_event_attr_t);
        pea.exclude_kernel = 1;
        pea.exclude_user = 0;
        pea.exclude_hv = 1;
        pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

    retry:
        int fd = -1;
        static const int kNumberOfTries = 5;
        for (int k = 0; k < kNumberOfTries; ++k) {
            fd = perf_event_open(&pea, pid, cpu, leader, flags);
            if (fd >= 0) break;
        }
        if (fd < 0) {
            if (leader >= 0) {
                leader = -1;
                goto retry;
            }
            int err = errno;
            std::cerr << "PerfGroup::init  Index:" << j << " errno:" << err << " "
                      << strerror(err) << "\n";
            return false;
        }
        if (leader < 0) {
            leader = fd;
            leaders.push_back(fd);
        }
        uint64_t id = 0;
        int result = ioctl(fd, PERF_EVENT_IOC_ID, &id);
        if (result < 0) {
            int err = errno;
            std::cerr << "PerfGroup::init ioctl(PERF_EVENT_IOC_ID) Index:" << j
                      << " errno:" << err << " " << strerror(err) << "\n";
            return false;
        }

        ids[j].fd = fd;
        ids[j].id = id;
        ids[j].order = j;
    }
    return true;
}

bool PerfGroup::init(const std::vector<std::string> &events) {
    std::vector<perf_event_attr_t> evds(events.size());
    std::vector<const char *> names(events.size());
    _ids.resize(events.size());
    for (size_t j = 0; j < events.size(); ++j) {
        names[j] = events[j].c_str();
        _ids[j].name = events[j];
    }
    if (!translate(names.data(), evds.data(), events.size())) return false;
    if (!::init(evds, _ids, _leaders)) return false;
    std::sort(_ids.begin(), _ids.end(), [](const Descriptor &lhs, const Descriptor &rhs) {
        return lhs.id < rhs.id;
    });
    _order.resize(_ids.size());
    for (size_t j = 0; j < _ids.size(); ++j) {
        _order[_ids[j].order] = j;
        _names[_ids[j].name] = j;
    }
    return true;
}

bool PerfGroup::start() {
    if (_ids.empty()) {
        std::cerr << "Group is empty!"
                  << "\n";
        return false;
    }
    for (int lead : _leaders) {
        int res = ioctl(lead, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
        if (res < 0) {
            int err = errno;
            std::cerr << "PerfGroup::start ioctl(PERF_EVENT_IOC_RESET) errno:" << err
                      << " " << strerror(err) << "\n";
            return false;
        }
    }
    for (int lead : _leaders) {
        int res = ioctl(lead, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
        if (res < 0) {
            int err = errno;
            std::cerr << "PerfGroup::init ioctl(PERF_EVENT_IOC_ENABLE) errno:" << err
                      << " " << strerror(err) << "\n";
            return false;
        }
    }
    return true;
}

bool PerfGroup::stop() {
    if (_ids.empty()) {
        std::cerr << "Group is empty!"
                  << "\n";
        return false;
    }
    for (int lead : _leaders) {
        int res = ioctl(lead, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
        if (res < 0) {
            int err = errno;
            std::cerr << "PerfGroup::init ioctl(PERF_EVENT_IOC_DISABLE) errno:" << err
                      << " " << strerror(err) << "\n";
            return false;
        }
    }
    read();
    return true;
}

size_t PerfGroup::size() const {
    return _ids.size();
}

uint64_t PerfGroup::operator[](size_t index) const {
    return _ids[index].value;
}

uint64_t PerfGroup::operator[](const char *name) const {
    auto it = _names.find(name);
    if (it == _names.end()) return std::numeric_limits<uint64_t>::max();
    return _ids[it->second].value;
}

std::string PerfGroup::name(size_t index) const {
    return _ids[index].name;
}

void PerfGroup::read() {
    size_t n = _ids.size();
    if (n == 0) return;
    struct MessageValue {
        uint64_t value;
        uint64_t id;
    };
    struct Message {
        uint64_t nr;
        MessageValue values[];
    };
    for (Descriptor &d : _ids) d.value = std::numeric_limits<uint64_t>::max();
    size_t bufsize = 2 * (sizeof(Message) + n * sizeof(MessageValue));
    std::vector<uint8_t> buf(bufsize);
    for (int lead : _leaders) {
        ssize_t nb = ::read(lead, buf.data(), bufsize);
        Message *msg = (Message *)buf.data();
        for (uint64_t i = 0; i < msg->nr; i++) {
            uint64_t id = msg->values[i].id;
            uint64_t value = msg->values[i].value;
            // std::cerr << "Read lead:" << lead << " index " << id << "/" << msg->nr << "
            // value " << value << "\n";
            auto it = std::lower_bound(
                _ids.begin(), _ids.end(), id,
                [](const Descriptor &d, size_t id) { return d.id < id; });
            if (it != _ids.end()) {
                if (id == it->id) {
                    it->value = value;
                }
            } else {
                std::cerr << "Not found id " << id << "\n";
            }
        }
    }
}
