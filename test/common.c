#include <stdio.h>

#include "FileMonitor.h"
#include "conveniences.h"

void printMonitors(const struct FMHandle *h, const char* title)
{
        int i = 0;
        printf(" |%s count:%d fd:%d" NL, title, h->count, h->inotify_fd);
        const struct FM* fm = NULL;
        while (NULL != (fm = FileMonitor_next(h, fm))) {
                printf(" | %i:%d %s" NL, i++, fm->wd, fm->path);
        }
}
