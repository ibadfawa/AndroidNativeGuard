#include "AntiDump.h"
#include "SecureAPI.h"
#include "Log.h"

#include <sys/inotify.h>
#include <fcntl.h>
#include <dirent.h>

const char *AntiDump::getName() {
    return "Memory Dump Detection";
}

eModuleSeverity AntiDump::getSeverity() {
    return HIGH;
}

bool AntiDump::execute() {
    int fd = SecureAPI::inotify_init1(0);
    if (fd < 0) {
        LOGI("AntiDump::execute inotify_init1 failed");
        return true;
    }

    int n = 0;
    int wd[100];

    wd[n++] = SecureAPI::inotify_add_watch(fd, "/proc/self/maps", IN_ACCESS | IN_OPEN);
    wd[n++] = SecureAPI::inotify_add_watch(fd, "/proc/self/mem", IN_ACCESS | IN_OPEN);
    wd[n++] = SecureAPI::inotify_add_watch(fd, "/proc/self/pagemap", IN_ACCESS | IN_OPEN);

    struct linux_dirent64 *dirp;
    char buf[512];
    int nread;

    int task = SecureAPI::openat(AT_FDCWD, "/proc/self/task", O_RDONLY | O_DIRECTORY, 0);
    while ((nread = SecureAPI::getdents64(task, (struct linux_dirent64 *) buf, sizeof(buf))) > 0) {
        for (int bpos = 0; bpos < nread;) {
            dirp = (struct linux_dirent64 *) (buf + bpos);
            if (!SecureAPI::strcmp(dirp->d_name, ".") || !SecureAPI::strcmp(dirp->d_name, "..") ) {
                bpos += dirp->d_reclen;
                continue;
            }
            if (dirp->d_type == DT_DIR) {
                char memPath[512], pagememPath[512];
                sprintf(memPath, "/proc/self/task/%s/mem", dirp->d_name);
                sprintf(pagememPath, "/proc/self/task/%s/pagemap", dirp->d_name);

                wd[n++] = SecureAPI::inotify_add_watch(fd, memPath, IN_ACCESS | IN_OPEN);
                wd[n++] = SecureAPI::inotify_add_watch(fd, pagememPath, IN_ACCESS | IN_OPEN);
            }
            bpos += dirp->d_reclen;
        }
    }
    SecureAPI::close(task);

    size_t len = SecureAPI::read(fd, buf, sizeof(buf));
    if (len) {
        struct inotify_event *event;
        for (char *ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
            event = (struct inotify_event *) ptr;
            if (event->mask & IN_ACCESS || event->mask & IN_OPEN) {
                LOGI("AntiDump::execute event->mask: %d", event->mask);
                SecureAPI::close(fd);
                return true;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        if (wd[i]) {
            SecureAPI::inotify_rm_watch(fd, wd[i]);
        }
    }

    SecureAPI::close(fd);
    return false;
}