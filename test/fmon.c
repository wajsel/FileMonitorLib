#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#include "FileMonitor.h"
#include "conveniences.h"

#define MAX_FILE_GROUPS 5


int onSetup(struct FMHandle *fm, const char* path)
{
        printf(GREEN "%s watched" RESET NL, path);

        return FM_OK;
}

int onUpdate(struct FMHandle *fm, const char* path)
{
        printf(YEL "%s updated" RESET NL, path);

        return FM_OK;
}

int onDelete(struct FMHandle *fm, const char* path)
{
        printf(RED "%s deleted" RESET NL, path);

        return FM_MONITOR; // keep path in among monitors
}


int doSelect(struct FMHandle fms[MAX_FILE_GROUPS], fd_set *rfds)
{
        struct timeval tv = {1, 0};
        int max_fd = 0;
        FD_ZERO(rfds);

        for (int i=0; i<MAX_FILE_GROUPS; i++) {
                const int fd = fms[i].inotify_fd;

                if (0 == fd) continue;

                FD_SET(fd, rfds);
                if (fd > max_fd) {
                        max_fd = fd;
                }
        }

        return select(max_fd + 1, rfds, NULL, NULL, &tv);
}

int indexFileUpdated(struct FMHandle *h, const char *path)
{
        /*
         * Easier implementation would be to just scrap the whole
         * group and add all files in the list (+ path itself)
         *
         * However that would trigger the onWachSetup callbacks each
         * time.
         */

        FILE *f = fopen(path, "r");
        char lines[FM_MAX_MONITORS -1][FM_PATH_MAX_LENGTH] = {0};
        char *p = *lines;

        size_t line_len = FM_PATH_MAX_LENGTH;
        ssize_t read_len = 0;
        int no_lines = 0;

        printf("Index %s updated" NL, path);

        if (!f) return FM_UNMONITOR;

        // get a list of paths
        while ((0 <= (read_len = getline(&p, &line_len, f))) &&
               (no_lines < FM_MAX_MONITORS -1)) {

                //printf("  %ld:%ld line:%s %d:%d" NL,
                //       read_len, line_len, p, p[read_len-1], '\n' );

                line_len = FM_PATH_MAX_LENGTH; // need to reset this

                // skip empty lines
                if (0 == read_len) continue;

                if (p[read_len-1] == '\n') {
                        p[read_len-1] = 0;
                }

                no_lines++;
                p = &lines[no_lines][0];
        }

        fclose(f);
        f = NULL;

        // Remove old monitors no longer in the list
        const struct FM* fm = NULL;
        while (NULL != (fm = FileMonitor_next(h, fm))) {
                bool found = false;
                for (int i=0; i<no_lines && !found; i++) {
                        if (0 == strcmp(fm->path, lines[i])) {
                                found = true;
                        }
                }
                if (!found) {
                        FileMonitor_unMonitor(h, fm->path);
                }
        }

        // Add the files in the index not already monitored
        for (int i=0; i<no_lines; i++) {
                if (!FileMonitor_isMonitored(h, lines[i])) {
                        FileMonitor_monitor(h, lines[i],
                                            onSetup, onUpdate, onDelete);
                }
        }

        FileMonitor_printMonitors(h, path);
        return FM_MONITOR;
}

int indexFileDeleted(struct FMHandle *h, const char *path)
{
        printf("index %s deleted, cleanup monitors" NL, path);

        const struct FM* fm = NULL;
        while (NULL != (fm = FileMonitor_next(h, fm))) {
                if (0 != strcmp(path, fm->path)) {
                        printf("UnMonitor %s" NL, fm->path);
                        FileMonitor_unMonitor(h, fm->path);
                }
        }

        return FM_MONITOR;
}

int main(int argc, char *argv[])
{
        fd_set rfds;
        struct FMHandle groups[MAX_FILE_GROUPS] = {0};

        printf("Limits: " NL
               " Max Path Length %d" NL
               " Max Monitors per group %d max" NL
               " Max Groups %d" NL,
               FM_PATH_MAX_LENGTH, FM_MAX_MONITORS, MAX_FILE_GROUPS);

        if (argc -1 > MAX_FILE_GROUPS) {
                fprintf(stderr, "Too many file groups %d max %d" NL,
                        argc, MAX_FILE_GROUPS);
                exit(1);
        }


        for (int i = 1, g = 0; i < argc; i++, g++) {
                printf("Adding %s to monitors group %d" NL, argv[i], g);

                FileMonitor_init(&groups[g]);

                FileMonitor_monitor(&groups[g], argv[i], indexFileUpdated,
                                    indexFileUpdated, indexFileDeleted);
        }

        for (;;) {

                const int err = doSelect(groups, &rfds);

                if (err > 0) {
                        for (int g=0; g<argc; g++) {
                                struct FMHandle *group = &groups[g];

                                if (FD_ISSET(group->inotify_fd, &rfds)) {
                                        FileMonitor_dispatch(group);
                                }
                        }
                }
                for (int g=0; g<argc; g++) {
                        FileMonitor_reMonitorNonExistingPaths(&groups[g]);
                }
        }

        return 0;
}
