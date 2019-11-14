#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "FileMonitor.h"
#include "conveniences.h"

int onSetup(struct FMHandle *fm, const char* path)
{
        printf(GREEN "%s created" RESET NL, path);

        return FM_MONITOR;
}

int onUpdate(struct FMHandle *fm, const char* path)
{
        printf(YEL "%s updated" RESET NL, path);
        printf("   Removing" NL);

        return FM_UNMONITOR;
}

int onDelete(struct FMHandle *fm, const char* path)
{
        printf(RED "%s deleted" RESET NL, path);

        return FM_MONITOR;
}

int doSelect(int fd, fd_set *rfds)
{
        struct timeval tv = {1};
        FD_ZERO(rfds);
        FD_SET(fd, rfds);

        return select(fd + 1, rfds, NULL, NULL, &tv);
}

int main(int argc, char *argv[])
{
        fd_set rfds;
        struct FMHandle fm = {0};
        int fd = FileMonitor_init(&fm);

        printf("Limits: Max Path Length %d, Max Monitors %d\n",
               FM_PATH_MAX_LENGTH, FM_MAX_MONITORS);

        // testing IN_NONBLOCK, without it the dispatch would hang
        FileMonitor_dispatch(&fm);

        for (int i = 1; i < argc; i++) {
                printf("Adding %s to monitors" NL, argv[i]);
                FileMonitor_monitor(&fm, argv[i], onSetup, onUpdate, onDelete);
        }
        
        FileMonitor_printMonitors(&fm, "Initial");
        for (;;) {

                const int err = doSelect(fd, &rfds);

                if (err > 0) {
                        // something happen to the monitored files
                        FileMonitor_dispatch(&fm);
                        FileMonitor_printMonitors(&fm, "");
                }
                else if (err == 0) {
                        // If other files are very buzzy this may
                        // happen very seldom

                        // TODO: accumulate time left from select and
                        // compare with a timeout
                        FileMonitor_reMonitorNonExistingPaths(&fm);
                }
                else {
                        return -1;
                }
        }
        
        return 0;
}
