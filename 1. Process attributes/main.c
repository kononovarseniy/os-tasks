#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/resource.h>

int str_to_rlim(const char *str, rlim_t *res) {
    char *end;
    errno = 0;
    rlim_t val = strtoul(str, &end, 10);
    // String contains invalid characters or an error occured.
    if (*end || errno)
        return -1;
    *res = val;
    return 0;
}

void print_ids() {
    uid_t uid = getuid(), euid = geteuid();
    gid_t gid = getgid(), egid = getegid();
    
    printf("uid: %d; euid: %d\n", uid, euid);
    printf("gid: %d; egid: %d\n", gid, egid);
}

void set_process_group() {
    printf("Setting proccess group id\n");
	if (setpgid(0, 0))
		perror("Unable to set process group id");
}

void print_process_ids() {
    pid_t pid = getpid(),
          ppid = getppid(),
          pgid = getpgrp();
    
    printf("pid: %d; parent: %d; group: %d\n", pid, ppid, pgid);
}

void print_resource_limit(int res, const char *name) {
    struct rlimit lim;
    if (getrlimit(res, &lim))
		perror("Unable to get resource limit");
    else
        printf("%s:\n\tsoft: %ld\n\thard: %ld\n", name, lim.rlim_cur, lim.rlim_max);
}

void set_resource_limit(int res, const char *name, rlim_t val) {
    printf("Setting %s\n", name);
    
    struct rlimit lim;
    if (getrlimit(res, &lim)) {
		perror("Unable to get resource limit");
		return;
    }
    
    lim.rlim_cur = val;
    if (setrlimit(res, &lim))
		perror("Unable to set resource limit");
}

static char path_buf[PATH_MAX];
void print_working_directory() {
    if (getcwd(path_buf, PATH_MAX) == NULL)
        perror("Unable to get current working directory");
    else
        printf("Current dir: %s\n", path_buf);
}

extern char **environ;
void print_vars() {
    printf("Environment variables:\n");
    for (char **p = environ; *p; p++)
        printf("\t%s\n", *p);
}

void put_var(const char *var) {
    printf("Setting %s\n", var);
    // The 'var' string will became part of the environment, so it needs to be copied.
    /*size_t len = strlen(var) + 1;
    char *copy = (char *)malloc(sizeof char * len);
    if (copy == NULL)
    {
        perror("Unable to allocate memory for environment variable");
        return;
    }
    strcpy(copy, var);*/
    char *copy = strdup(var);
    if (copy == NULL) {
        perror("Unable to set environment variable");
        return;
    }
    if (putenv(copy))
        perror("Unable to set environment variable");
}

int main(int argc, char * const argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, ":ispuU:cC:dvV:")) != -1) {
        rlim_t val;
        
        switch (opt) {
            case 'i':
                print_ids();
                break;
                
            case 's':
                set_process_group();
                break;
                
            case 'p':
                print_process_ids();
                break;
                
            case 'u':
                print_resource_limit(RLIMIT_FSIZE, "File size limit");
                break;
                
            case 'U':
                if (str_to_rlim(optarg, &val))
                    fprintf(stderr, "\"%s\" is invalid ulimit value\n", optarg);
                else
                    set_resource_limit(RLIMIT_FSIZE, "file size limit", val);
                break;
                
            case 'c':
                print_resource_limit(RLIMIT_CORE, "Core-file size limit");
                break;
                
            case 'C':
                if (str_to_rlim(optarg, &val))
                    fprintf(stderr, "\"%s\" is invalid core-file size limit value\n", optarg);
                else
                    set_resource_limit(RLIMIT_CORE, "core-file size limit", val);
                break;
              
            case 'd':
                print_working_directory();
                break;
            
            case 'v':
                print_vars();
                break;
                
            case 'V':
                put_var(optarg);
                break;
            
            case ':': // Missing arg
                printf("Missing argument for option -%c\n", (char)optopt);
                break;
            
            case '?':
            default: // Unknown option
                printf("Unrecognized option -%c\n", (char)optopt);
                break;
        }
    }
    return 0;
}
