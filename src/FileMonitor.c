/**
 * Monitor files using inotify
 * see INOTIFY(7)
 */

#include <sys/inotify.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <stdio.h>
#include <errno.h>

#include "FileMonitor.h"

#ifdef DEBUG
#define WATCH_MASK IN_ALL_EVENTS
#else
#define WATCH_MASK (IN_DELETE_SELF | IN_CLOSE_WRITE)
#endif

#define NL "\n"

static void remove_monitor(struct FMHandle *h, struct FM* fm)
{
        if (-1 != fm->wd) {
                inotify_rm_watch(h->inotify_fd, fm->wd);
        }
        memset(fm, 0, sizeof(*fm));
        fm->wd = -1;
}

static struct FM* findWd(struct FM *monitors, int array_length, int wd)
{
        while (array_length--) {
                if (wd == monitors->wd) return monitors;
                monitors++;
        }

        return NULL;
}

static struct FM* findPath(struct FM *monitors, int array_length, const char* path)
{
        while (array_length--) {
                if (monitors->path[0] &&
                    (0 == strncmp(path, monitors->path, FM_PATH_MAX_LENGTH))) {
                        return monitors;
                }

                monitors++;
        }

        return NULL;
}

static void handleEvent(struct FMHandle *h, const struct inotify_event* event)
{
        struct FM *fm = findWd(h->monitors, FM_MAX_MONITORS, event->wd);
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

        for (int i=0; i < FM_MAX_MONITORS; i++) {
                h->monitors[i].wd = -1;
        }
        h->inotify_fd = inotify_init1(IN_NONBLOCK);
        h->next_index = 0;

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
                struct FM *fm = findPath(h->monitors, FM_MAX_MONITORS, path);
                if (NULL == fm) {
                        // not a known path so we use a new slot in
                        // the monitors array
                        fm = &h->monitors[h->next_index];
                        h->count++;

                }

                fm->wd = wd;
                fm->onWatchSetup = onWatchSetup;
                fm->onUpdate = onUpdate;
                fm->onDelete = onDelete;
                strncpy(fm->path, path, FM_PATH_MAX_LENGTH-1);

                // find a new next index
                int i = h->next_index;
                for (; i < FM_MAX_MONITORS; i++) {
                        if (0 == h->monitors[i].path[0]) {

                                h->next_index = i;
                                break;
                        }
                }
                if (i == FM_MAX_MONITORS) {
                        for (i = 0; i < h->next_index; i++) {
                                if (0 == h->monitors[i].path[0]) {

                                        h->next_index = i;
                                        break;
                                }
                        }
                }
        }

        if ((-1 != wd) && onWatchSetup) {
                onWatchSetup(h, path);
        }

        return rv;
}

int FileMonitor_unMonitor(struct FMHandle *h, const char *path)
{
        int rv = -1;

        if (!h || (0 > h->inotify_fd) || !path) return rv;

        rv = 0;
        struct FM *fm = findPath(h->monitors, FM_MAX_MONITORS, path);
        if (fm) {
                remove_monitor(h, fm);
                rv = 1;
                h->count--;
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

bool FileMonitor_hasNonExistingPaths(const struct FMHandle *h)
{
        if (!h || (0 > h->inotify_fd) || h->count == 0) return false;

        for (int i=0; i < FM_MAX_MONITORS; i++) {
                const struct FM *fm = &h->monitors[i];

                if ((fm->path[0] != 0) && (-1 == fm->wd)) {
                        return true;
                }
        }
        return false;
}

void FileMonitor_reMonitorNonExistingPaths(struct FMHandle *h)
{
        if (!h || (0 > h->inotify_fd) || h->count == 0) return;

        for (int i=0; i < FM_MAX_MONITORS; i++) {
                struct FM *fm = &h->monitors[i];

                if ((fm->path[0] != 0) && (-1 == fm->wd)) {

                        int wd = inotify_add_watch(h->inotify_fd, fm->path,
                                                   WATCH_MASK);

                        fm->wd = wd;
                        if ((-1 != wd) && (fm->onWatchSetup)) {
                                if (FM_UNMONITOR == fm->onWatchSetup(h, fm->path)) {
                                        remove_monitor(h, fm);
                                }
                        }
                }
        }
}

void FileMonitor_printMonitors(const struct FMHandle *h, const char* title)
{
        if (!h) return;

        printf("%s:%d\n", title, h->inotify_fd);
        for (int i=0; i < FM_MAX_MONITORS; i++) {
                const struct FM *fm = &h->monitors[i];

                if (0 == fm->path[0]) continue;

                printf("%i:%d %s\n", i, fm->wd, fm->path);
        }
        printf("next index:%d\n", h->next_index);
}

bool FileMonitor_isMonitored(struct FMHandle *h, const char* path)
{
        return (NULL != findPath(h->monitors, FM_MAX_MONITORS, path));
}

const struct FM * FileMonitor_next(const struct FMHandle *h, const struct FM *fm)
{
        if (!h) return NULL;

        const struct FM* start = (NULL == fm) ? &h->monitors[0] : fm;

        while (start++ != &h->monitors[FM_MAX_MONITORS]) {
                if (0 != start->path[0]) {
                        return start;
                }
        }
        return NULL;
}