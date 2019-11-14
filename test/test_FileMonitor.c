#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "FileMonitor.h"

#include "conveniences.h"

struct State {
        fd_set rfds;
        struct timeval tv;
        int fd;
};


#define PATH   "data/watchedFile.txt"
#define PATH_2 "data/watchedFile_2.txt"
#define PATH_3 "data/watchedFile_3.txt"
#define PATH_NOT_EXISTING   "data/nonExistingFile"
#define PATH_NOT_EXISTING_2 "data/nonExistingFile_2"
#define PATH_NOT_EXISTING_3 "data/nonExistingFile_3"

static int doSelect(int fd, fd_set *rfds)
{
        struct timeval tv = {0, 100};
        FD_ZERO(rfds);
        FD_SET(fd, rfds);

        return select(fd + 1, rfds, NULL, NULL, &tv);
}

void testFM_setup(void **state)
{
        struct State *s = test_malloc(sizeof(*s));
        FD_ZERO(&s->rfds);
        s->tv.tv_sec = 1;
        s->tv.tv_usec = 0;
        s->fd = -1;

        (void)system("mkdir -p data/");
        (void)system("touch " PATH);
        (void)system("touch " PATH_2);
        (void)system("touch " PATH_3);

        remove(PATH_NOT_EXISTING);
        remove(PATH_NOT_EXISTING_2);
        remove(PATH_NOT_EXISTING_3);

        *state = s;
}

void testFM_teardown(void **state)
{
        if (*state) {
                test_free(*state);
        }
}

/* Callbacks */
static int onUpdate(struct FMHandle* h, const char* path)
{
        printf(GREEN "%s :: %s" RESET NL, __FUNCTION__, path);

        check_expected(path);

        return FM_MONITOR;
}

static int onDelete(struct FMHandle* h, const char* path)
{
        printf(RED "%s :: %s" RESET NL, __FUNCTION__, path);

        check_expected(path);

        return FM_MONITOR;
}

int onWatchSetup(struct FMHandle* h, const char* path)
{
        printf(YEL "%s :: %s" RESET NL, __FUNCTION__, path);

        check_expected(path);

        return FM_MONITOR;
}

int onDelete_reMonitor(struct FMHandle* h, const char* path)
{
        printf(RED "%s :: %s" RESET NL, __FUNCTION__, path);

        check_expected(path);

        return FM_MONITOR;
}


void testFM_init(void **state)
{
        struct FMHandle fm = {0};
        int fd = FileMonitor_init(&fm);
        assert_int_equal(fd, fm.inotify_fd);
        assert_true(0 < fm.inotify_fd);
}

void testFM_monitor(void **state)
{
        struct FMHandle fm = {0};
        FileMonitor_init(&fm);

        expect_string(onWatchSetup, path, PATH);

        int err = FileMonitor_monitor(&fm, PATH, onWatchSetup, NULL, NULL);

        assert_int_equal(1, err);
        assert_int_not_equal(-1, fm.monitors[0].wd);
        assert_int_equal(1, fm.count);
}

void testFM_monitorReentrant(void **state)
{
        struct FMHandle fm = {0};
        FileMonitor_init(&fm);

        expect_string_count(onWatchSetup, path, PATH, 2);

        FileMonitor_monitor(&fm, PATH, onWatchSetup, NULL, NULL);
        FileMonitor_monitor(&fm, PATH, onWatchSetup, NULL, NULL);

        // only one monitor was created
        assert_int_equal(1, fm.count);
}

void testFM_monitorNonExistent(void **state)
{
        struct FMHandle fm = {0};
        FileMonitor_init(&fm);

        int err = FileMonitor_monitor(&fm, PATH_NOT_EXISTING,
                                      onWatchSetup, NULL, NULL);

        assert_int_equal(0, err);
        assert_int_equal(-1, fm.monitors[0].wd);
}

void testFM_monitorTooMany(void **state)
{
        int i;
        struct FMHandle fm = {0};
        FileMonitor_init(&fm);

        for (i=0; i < FM_MAX_MONITORS; i++) {
                char path[10] = {0};
                snprintf(path, sizeof(path) -1, "path_%d", i);
                assert_int_equal(0, FileMonitor_monitor(&fm, path, NULL,NULL,NULL));
        }
        char path[10] = {0};
        snprintf(path, sizeof(path) -1, "path_%d", i);
        assert_int_equal(FM_MAX_MONITORS, i);
        assert_int_equal(-1, FileMonitor_monitor(&fm, path, NULL, NULL,NULL));
}

void testFM_unMonitor(void **state)
{
        struct FMHandle fm = {0};
        FileMonitor_init(&fm);
        FileMonitor_monitor(&fm, PATH, NULL, NULL, NULL);
        assert_int_equal(1, fm.count);

        int next_index = fm.next_index;
        assert_int_equal(1, FileMonitor_unMonitor(&fm, PATH));
        assert_int_equal(0, fm.count);
        // next_index is not affected by unMonitor()
        assert_int_equal(fm.next_index, next_index);
}

void testFM_unMonitorNotMonitored(void **state)
{
        struct FMHandle fm = {0};
        FileMonitor_init(&fm);
        FileMonitor_monitor(&fm, PATH, NULL, NULL, NULL);

        assert_int_equal(0, FileMonitor_unMonitor(&fm, PATH_NOT_EXISTING));
}

void testFM_onWatchSetup(void **state)
{
        struct FMHandle fm = {0};
        FileMonitor_init(&fm);

        expect_string(onWatchSetup, path, PATH);
        FileMonitor_monitor(&fm, PATH, onWatchSetup, NULL, NULL);
}

void testFM_onUpdate(void **state)
{
        struct State *s = *state;

        struct FMHandle fm = {0};
        FileMonitor_init(&fm);
        FileMonitor_monitor(&fm, PATH, NULL, onUpdate, NULL);

        FD_SET(fm.inotify_fd, &s->rfds);

        system("echo apa > " PATH);

        expect_string(onUpdate, path, PATH);

        int err = select(fm.inotify_fd + 1, &s->rfds, NULL, NULL, &s->tv);

        assert_int_not_equal(0, err);
        assert_true(FD_ISSET(fm.inotify_fd, &s->rfds));
        FileMonitor_dispatch(&fm);
}

void testFM_onUpdate3Files(void **state)
{
        struct State *s = *state;

        struct FMHandle fm = {0};
        s->fd = FileMonitor_init(&fm);

        FileMonitor_monitor(&fm, PATH, NULL, onUpdate, NULL);
        FileMonitor_monitor(&fm, PATH, NULL, onUpdate, NULL);
        FileMonitor_monitor(&fm, PATH, NULL, onUpdate, NULL);
        FileMonitor_monitor(&fm, PATH_2, NULL, onUpdate, NULL);
        FileMonitor_monitor(&fm, PATH_3, NULL, onUpdate, NULL);

        FD_SET(s->fd, &s->rfds);

        system("echo apa > " PATH);
        system("echo bpa > " PATH_2);
        system("echo cpa > " PATH_3);

        expect_string(onUpdate, path, PATH);
        expect_string(onUpdate, path, PATH_2);
        expect_string(onUpdate, path, PATH_3);

        int err = select(s->fd + 1, &s->rfds, NULL, NULL, &s->tv);

        assert_int_not_equal(0, err);
        assert_true(FD_ISSET(s->fd, &s->rfds));
        FileMonitor_dispatch(&fm);
}

void testFM_onDelete(void **state)
{
        struct State *s = *state;

        struct FMHandle fm = {0};
        FileMonitor_init(&fm);
        FileMonitor_monitor(&fm, PATH, NULL, NULL, onDelete);

        FD_SET(fm.inotify_fd, &s->rfds);

        remove(PATH);

        expect_string(onDelete, path, PATH);

        int err = select(fm.inotify_fd + 1, &s->rfds, NULL, NULL, &s->tv);

        assert_int_not_equal(0, err);
        assert_true(FD_ISSET(fm.inotify_fd, &s->rfds));
        FileMonitor_dispatch(&fm);
}

void testFM_hasNonExistingPathsTrue(void **state)
{
        struct FMHandle fm = {0};
        FileMonitor_init(&fm);
        FileMonitor_monitor(&fm, PATH_NOT_EXISTING, NULL, NULL, NULL);
        assert_true(FileMonitor_hasNonExistingPaths(&fm));
}

void testFM_hasNonExistingPathsFalse(void **state)
{
        struct FMHandle fm = {0};
        FileMonitor_init(&fm);
        FileMonitor_monitor(&fm, PATH, NULL, NULL, NULL);
        assert_false(FileMonitor_hasNonExistingPaths(&fm));
}

void testFM_hasNonExistingPathsFalse2(void **state)
{
        struct FMHandle fm = {0};
        FileMonitor_init(&fm);
        assert_false(FileMonitor_hasNonExistingPaths(&fm));
}

void testFM_detectFileCreation(void **state)
{
        struct State *s = *state;

        struct FMHandle fm = {0};
        FileMonitor_init(&fm);
        FileMonitor_monitor(&fm, PATH_NOT_EXISTING, onWatchSetup, onUpdate, NULL);

        assert_true(FileMonitor_hasNonExistingPaths(&fm));
        system("echo apa > " PATH_NOT_EXISTING);

        expect_string(onWatchSetup, path, PATH_NOT_EXISTING);
        FileMonitor_reMonitorNonExistingPaths(&fm);

        system("echo apa >> " PATH_NOT_EXISTING);

        int err = doSelect(fm.inotify_fd, &s->rfds);

        assert_int_not_equal(0, err);
        expect_string(onUpdate, path, PATH_NOT_EXISTING);
        FileMonitor_dispatch(&fm);
}

void testFM_detectFileCreatedMulti(void **state)
{
        struct State *s = *state;

        struct FMHandle fm = {0};
        FileMonitor_init(&fm);
        FileMonitor_monitor(&fm, PATH_NOT_EXISTING, onWatchSetup, onUpdate, NULL);
        FileMonitor_monitor(&fm, PATH_NOT_EXISTING_2, onWatchSetup, onUpdate, NULL);
        FileMonitor_monitor(&fm, PATH_NOT_EXISTING_3, onWatchSetup, onUpdate, NULL);

        assert_true(FileMonitor_hasNonExistingPaths(&fm));
        system("echo apa > " PATH_NOT_EXISTING_2);

        expect_string(onWatchSetup, path, PATH_NOT_EXISTING_2);
        FileMonitor_reMonitorNonExistingPaths(&fm);

        system("echo apa >> " PATH_NOT_EXISTING_2);

        int err = doSelect(fm.inotify_fd, &s->rfds);

        assert_int_not_equal(0, err);
        expect_string(onUpdate, path, PATH_NOT_EXISTING_2);
        FileMonitor_dispatch(&fm);
}

void testFM_addMonitorOnDelete(void **state)
{
        struct State *s = *state;

        struct FMHandle fm = {0};
        FileMonitor_init(&fm);
        expect_string(onWatchSetup, path, PATH);
        FileMonitor_monitor(&fm, PATH, onWatchSetup, onUpdate, onDelete_reMonitor);

        // detect delete
        remove(PATH);

        doSelect(fm.inotify_fd, &s->rfds);

        expect_string(onDelete_reMonitor, path, PATH);
        FileMonitor_dispatch(&fm);

        // detect existance
        system("touch " PATH);
        expect_string(onWatchSetup, path, PATH);
        FileMonitor_reMonitorNonExistingPaths(&fm);

        // detect update
        system("echo apa > " PATH);

        doSelect(fm.inotify_fd, &s->rfds);

        expect_string(onUpdate, path, PATH);
        FileMonitor_dispatch(&fm);
}

void testFM_nonblocking(void **state)
{
        struct FMHandle fm = {0};
        FileMonitor_init(&fm);
        expect_string(onWatchSetup, path, PATH);
        FileMonitor_monitor(&fm, PATH, onWatchSetup, onUpdate, onDelete_reMonitor);

        // this should hang if inotify is not init with IN_NONBLOCK

        FileMonitor_dispatch(&fm);
}
