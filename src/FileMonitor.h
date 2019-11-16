/**
 * Monitor one or several file paths using a monitoring file
 * descriptor.
 *
 * Only react to IN_DELETE_SELF and IN_CLOSE_WRITE from inotify(7)
 * Moving files will probably mess things up.
 *
 * When select markes the monitoring fd as readable, dispatch
 * callbacks via the FileMonitor_dispatch() function.
 *
 * The following events are detected
 *  - onChange
 *    File has been written and closed
 *
 *  - onDelete
 *    File has been deleted
 *
 *    The handler of this event may re-add a monitor on the path to
 *    detect if the file is again created.
 *
 *  - onWatchSetup
 *    Watch is setup successfully.
 *
 *    This is also called when a watch is setup for a non-existing
 *    file which existance is detected with the
 *    reMonitorNonExistingPaths() function.
 *
 *    Read the initial file content on this event.
 *
 * Each event handler receives a the handle and the path monitored.
 */

#ifndef __FILE_MONITOR_H__
#define __FILE_MONITOR_H__

#include <stdbool.h>

#ifndef FM_PATH_MAX_LENGTH
#define FM_PATH_MAX_LENGTH 256
#endif

#ifndef FM_MAX_MONITORS
#define FM_MAX_MONITORS 10
#endif

struct FMHandle;

typedef int(*FMOnWatchSetup)(struct FMHandle* h, const char* path);
typedef int(*FMOnUpdate)(struct FMHandle* h, const char* path);
typedef int(*FMOnDelete)(struct FMHandle* h, const char* path);


struct FM {
        int wd;
        char path[FM_PATH_MAX_LENGTH];
        FMOnWatchSetup onWatchSetup;
        FMOnUpdate onUpdate;
        FMOnDelete onDelete;
};

enum FMStatus {
        FM_UNMONITOR = -1,
        FM_MONITOR = 0,
        FM_OK
};



/**
 * Handle to the API
 */
struct FMHandle {
        int inotify_fd;

        // INTERNAL BELOW

        struct FM monitors[FM_MAX_MONITORS];
        int count;
};

/**
 * Initialize a handle
 *
 * h->inotify_fd can later be selected on, followed by a call of
 * dispatch()
 *
 * return inotify_fd
 */
int FileMonitor_init(struct FMHandle *h);

/**
 * Monitor path
 *
 * return -1 on failure
 *  - handle is null
 *  - handle is not initialized with init()
 *  - path is null
 *  - no more empty slots for monitoring files
 *    Max number of files to monitor is FM_MAX_MONITORS
 *
 * return 0 if adding watch failed.
 *   Perhaps the path did not exist.
 *
 * return 1 if path is now monitored
 */
int FileMonitor_monitor(struct FMHandle *handle, const char *path,
                        FMOnWatchSetup onWatchSetup,FMOnUpdate onUpdate,
                        FMOnDelete onDelete);

/**
 * Stop monitor path
 *
 * This also removes the whole entry and a new monitor() call is
 * needed if the path should be monitored again.
 *
 * return -1 on failure
 *  - handle is null,
 *  - handle is not initialized with init()
 *  - path is null
 *
 * return 0 if not a failure
 *
 * return 1 if path was actually unmonitored
 */
int FileMonitor_unMonitor(struct FMHandle *h, const char *path);

/**
 * Read events from inotify file descriptor and call eventhandels.
 *
 * Monitors may be removed depending on the return code from the event
 * handlers.
 */
void FileMonitor_dispatch(struct FMHandle *h);

/**
 * Does monitors exist for which the paths does not exist?
 *
 * Could happen when a file is removed and handler does not return
 * FM_UNMONITOR. Or the path does not exist when calling monitor().
 *
 * return number of non-existing paths or -1 if h is not setup properly
 */
int FileMonitor_nonExistingPaths(const struct FMHandle *h);

/**
 * Check if paths that previously did not exist now exists and update
 * the monitor.
 */
// TODO: bad naming
void FileMonitor_reMonitorNonExistingPaths(struct FMHandle *h);

/**
 * Return true if path is found among monitors
 */
bool FileMonitor_isMonitored(struct FMHandle *h, const char* path);

/**
 * Iterator
 *
 * Given a handler and an FM* pointing into the monitors array,
 * return a pointer to the next FM in the monitor array which has a
 * non-empty path
 *
 * Called with fm == NULL will start iterate from beginning of monitors
 *
 * return NULL as an iteration end condition
 */
const struct FM * FileMonitor_next(const struct FMHandle *h, const struct FM *fm);

#endif
