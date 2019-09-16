#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

extern char *tzname[];

void print_time(const time_t *t, char *tz_var) {
    struct tm *sp;
    
    if (putenv(tz_var)) {
        perror("Unable to set environment variable");
        return;
    }
    
    sp = localtime(t);
    printf("localtime (%s):\n\t", tz_var);
    printf("%d/%d/%02d %d:%02d %s\n",
        sp->tm_mon + 1, sp->tm_mday,
        (sp->tm_year % 100), sp->tm_hour,
        sp->tm_min, tzname[sp->tm_isdst]);
}

int main()
{
    time_t now;

    (void) time( &now );
    printf("ctime: %s", ctime( &now ) );

    print_time(&now, "TZ="); // UTC
    print_time(&now, "TZ=<+07>-7");
    print_time(&now, "TZ=:Asia/Novosibirsk");
    print_time(&now, "TZ=PST8PDT");
    print_time(&now, "TZ=:America/Los_Angeles");
    
    return 0;
}
