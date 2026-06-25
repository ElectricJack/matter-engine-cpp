#include "inotify_watcher.h"
#ifdef __linux__
#include <sys/inotify.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <climits>
#include <unordered_map>
#include <cstring>

namespace live_edit {

// wd->dir lets us rebuild absolute paths from per-event file names. Stored as a
// member (declared `void* dirs_` in the header to keep <unordered_map> out of
// it); accessed via this helper.
static std::unordered_map<int, std::string>& wd_map(void*& opaque) {
    if (!opaque) opaque = new std::unordered_map<int, std::string>();
    return *static_cast<std::unordered_map<int, std::string>*>(opaque);
}

InotifyWatcher::InotifyWatcher() {
    fd_ = inotify_init1(IN_NONBLOCK);
}
InotifyWatcher::~InotifyWatcher() {
    if (fd_ >= 0) ::close(fd_);
    delete static_cast<std::unordered_map<int, std::string>*>(dirs_);
}

void InotifyWatcher::add_watch(const std::string& dir) {
    if (fd_ < 0) return;
    int wd = inotify_add_watch(fd_, dir.c_str(),
                               IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
    if (wd >= 0) wd_map(dirs_)[wd] = dir;
}

int InotifyWatcher::poll(std::vector<FileEvent>& out) {
    if (fd_ < 0) return 0;
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    int added = 0;
    for (;;) {
        ssize_t len = ::read(fd_, buf, sizeof(buf));
        if (len <= 0) break;  // EAGAIN (NONBLOCK) or EOF -> done draining
        for (char* p = buf; p < buf + len; ) {
            auto* ev = reinterpret_cast<struct inotify_event*>(p);
            if (ev->len > 0) {
                auto it = wd_map(dirs_).find(ev->wd);
                std::string dir = (it == wd_map(dirs_).end()) ? std::string() : it->second;
                std::string path = dir.empty() ? std::string(ev->name) : dir + "/" + ev->name;
                FileEventKind k = FileEventKind::Modified;
                if (ev->mask & (IN_CREATE | IN_MOVED_TO))      k = FileEventKind::Created;
                else if (ev->mask & (IN_DELETE | IN_MOVED_FROM)) k = FileEventKind::Deleted;
                out.push_back(FileEvent{path, k, now_ms()});
                ++added;
            }
            p += sizeof(struct inotify_event) + ev->len;
        }
    }
    return added;
}

long long InotifyWatcher::now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

} // namespace live_edit
#endif // __linux__
