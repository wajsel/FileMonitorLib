/**
 * Monitor files using inotify
 * see INOTIFY(7)
 */

#include <sys/inotify.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <errno.h>

#include "FileMonitor.h"

#ifdef DEBUG
#include <stdio.h>
#define NL "\n"
#define WATCH_MASK IN_ALL_EVENTS
#else
#define WATCH_MASK (IN_DELETE_SELF | IN_CLOSE_WRITE)
#endif


#define FOR(x)                                  \
        for (struct FM *fm = x; fm < (x + FM_MAX_MONITORS); ++fm)

#define FOR_CONST(x)                            \
        for (const struct FM *fm = x; fm < (x + FM_MAX_MONITORS); ++fm)

static void remove_monitor(struct FMHandle *h, struct FM* fm)
{
        if (-1 != fm->wd) {
                inotify_rm_watch(h->inotify_fd, fm->wd);
        }
        memset(fm, 0, sizeof(*fm));
        fm->wd = -1;
        --h->count;
}

static struct FM* findWd(struct FMHandle *h, const int wd)
{
        FOR (h->monitors) {
                if (wd == fm->wd) {return fm;}
        }

        return NULL;
}

static struct FM* findPath(struct FMHandle *h, const char* path)
{
        FOR (h->monitors) {
                if (fm->path[0] &&
                    (0 == strncmp(path, fm->path, FM_PATH_MAX_LENGTH))) {
                        return fm;
                }
        }

        return NULL;
}

static void handleEvent(struct FMHandle *h, const struct inotify_event* event)
{
        struct FM *fm = findWd(h, event->wd);
        if (fm) {
#ifdef DEBUG
                printf("%s"NL, fm->path);
                if (event->mask & IN_ACCESS) {printf(" IN_ACCESS" NL);}
                else if (event->mask & IN_ATTRIB) {printf(" IN_ATTRIB" NL);}
                else if (event->mask & IN_CLOSE_WRITE) {printf(" CLOSE_WRITE" NL);}
                else if (event->mask & IN_CLOSE_NOWRITE) {printf(" IN_CLOSE_NOWRITE" NL);}
                else if (event->mask & IN_CREATE) {printf(" IN_CREATE" NL);}
                else if (event->mask & IN_DELETE) {printf(" IN_DELETE" NL);}
                else if (event->mask & IN_DELETE_SELF) {printf(" IN_DELETE_SELF" NL);}
                else if (event->mask & IN_MODIFY) {printf(" IN_MODIFY" NL);}
                else if (event->mask & IN_MOVE_SELF) {printf(" IN_MOVE_SELF" NL);}
                else if (event->mask & IN_MOVED_FROM) {printf(" IN_MOVED_FROM" NL);}
                else if (event->mask & IN_MOVED_TO) {printf(" IN_MOVED_TO" NL);}
                else if (event->mask & IN_OPEN) {printf(" IN_OPEN" NL);}
#endif

                if ((event->mask & IN_DELETE_SELF) &&
                    fm->onDelete) {

                        if (FM_MONITOR != fm->onDelete(h, fm->path)) {
                                remove_monitor(h, fm);
                        }
                        else {
                                inotify_rm_watch(h->inotify_fd, fm->wd);
                                fm->wd = -1;
                        }
                }
                else if ((event->mask & IN_CLOSE_WRITE) &&
                         fm->onUpdate) {

                        if (FM_UNMONITOR == fm->onUpdate(h, fm->path)) {
                                remove_monitor(h, fm);
                        }
                }
        }
}

int FileMonitor_init(struct FMHandle *h)
{
        if (!h) return -1;

        // we can not know if h is already initialized or just not
        // zero-initialized => scrap the hole thing
        memset(h, 0, sizeof(*h));

        FOR(h->monitors) {fm->wd = -1;}

        h->inotify_fd = inotify_init1(IN_NONBLOCK);

        return h->inotify_fd;
}

int FileMonitor_monitor(struct FMHandle *h, const char *path,
                        FMOnWatchSetup onWatchSetup, FMOnUpdate onUpdate,
                        FMOnDelete onDelete)
{
        int rv = -1;

        if (!h || (0 > h->inotify_fd)) return -1;
        if (h->count >= FM_MAX_MONITORS) return -1;
        if (!path) return -1;

        int wd = inotify_add_watch(h->inotify_fd, path, WATCH_MASK);

        if (0 > wd) {
                if (errno == ENOENT) {
                        // indicate that path is not monitored
                        rv = 0;
                }
        }
        else {
                rv = 1;
        }

        if (-1 != rv) {
                // check if its a known path
                bool found_existing = false;
                struct FM *new_fm = NULL;
                FOR (h->monitors) {
                        if (0 == fm->path[0]) {
                                new_fm = fm;
                        }
                        else if (0 == strcmp(path, fm->path)) {
                                new_fm = fm;
                                found_existing = true;
                                break;
                        }
                }
                if (!found_existing) {++h->count;}

                new_fm->wd = wd;
                new_fm->onWatchSetup = onWatchSetup;
                new_fm->onUpdate = onUpdate;
                new_fm->onDelete = onDelete;
                strncpy(new_fm->path, path, FM_PATH_MAX_LENGTH-1);

                if ((-1 != wd) && onWatchSetup) {
                        if (FM_UNMONITOR == onWatchSetup(h, new_fm->path)) {
                                remove_monitor(h, new_fm);
                        }
                }
        }
        return rv;
}

int FileMonitor_unMonitor(struct FMHandle *h, const char *path)
{
        int rv = -1;

        if (!h || (0 > h->inotify_fd) || !path) return rv;

        rv = 0;
        struct FM *fm = findPath(h, path);
        if (fm) {
                remove_monitor(h, fm);
                rv = 1;
        }
        return rv;
}

void FileMonitor_dispatch(struct FMHandle *h)
{
        if (!h || (0 > h->inotify_fd) || h->count == 0) return;

        // FIONREAD should return the number of bytes to read from the
        // inotify_fd. Can this be used here to deplete the events
        // instead of relying on the external select? man ioctl(2)
        struct inotify_event *event = NULL;
        char buf[128] = {0};
        char *next_event_ptr = NULL;

        const int numRead = read(h->inotify_fd, buf, sizeof(buf));

        for (next_event_ptr = buf;
             next_event_ptr < buf + numRead;
             next_event_ptr += sizeof(*event) + event->len) {

                event = (struct inotify_event *)next_event_ptr;
                handleEvent(h, event);
        }

}

int FileMonitor_nonExistingPaths(const struct FMHandle *h)
{
        if (!h || (0 > h->inotify_fd)) return -1;
        if (h->count == 0) return 0;

        int count = 0;
        FOR_CONST (h->monitors) {
                if ((fm->path[0] != 0) && (-1 == fm->wd)) {
                        ++count;
                }
        }
        return count;
}

void FileMonitor_reMonitorNonExistingPaths(struct FMHandle *h)
{
        if (!h || (0 > h->inotify_fd) || h->count == 0) return;

        FOR (h->monitors) {
                if ((fm->path[0] != 0) && (-1 == fm->wd)) {

                        fm->wd = inotify_add_watch(h->inotify_fd, fm->path,
                                                   WATCH_MASK);

                        if ((-1 != fm->wd) && (fm->onWatchSetup)) {
                                if (FM_UNMONITOR == fm->onWatchSetup(h, fm->path)) {
                                        remove_monitor(h, fm);
                                }
                        }
                }
        }
}

bool FileMonitor_isMonitored(struct FMHandle *h, const char* path)
{
        if (!h || !path) return false;

        return (NULL != findPath(h, path));
}

const struct FM * FileMonitor_next(const struct FMHandle *h, const struct FM *fm)
{
        if (!h) return NULL;

        const struct FM* start = (NULL == fm) ? &h->monitors[0] : ++fm;

        while (start != &h->monitors[FM_MAX_MONITORS]) {
                if (0 != start->path[0]) {
                        return start;
                }
                ++start;
        }
        return NULL;
}
