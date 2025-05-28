
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "block.h"
#include "common.h"
#include "fs.h"
#include "log.h"

// global variables
int ncyl, nsec;

#define ReplyYes()       \
    do {                 \
        printf("Yes\n"); \
        Log("Success");  \
    } while (0)
#define ReplyNo(x)      \
    do {                \
        printf("No\n"); \
        Warn(x);        \
    } while (0)
#define ParseArgs(maxargs)      \
    char *argv[maxargs + 1];    \
    int argc = parse(args, argv, maxargs);

int parse(char *line, char *argv[], int maxargs) {
    char *p;
    int argc = 0;
    p = strtok(line, " \r\n");
    while (p) {
        argv[argc++] = p;
        if (argc >= maxargs) break;
        p = strtok(NULL, " \r\n");
    }
    if (argc >= maxargs) {
        argv[argc] = p + strlen(p) + 1;
    } else {
        argv[argc] = NULL;
    }
    return argc;
}


int handle_f(char *args) {
    if (cmd_f(ncyl, nsec) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to format");
    }
    return 0;
}

int handle_mk(char *args) {
    ParseArgs(MAXARGS);
    if (argc < 1) {
        ReplyNo("Usage: mk <filename>");
        return 0;
    }
    char *name = argv[0];
    short mode = argc >= 2 ? (atoi(argv[1]) & 0b1111) : 0b1110;

    if (cmd_mk(name, mode) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to create file");
    }
    return 0;
}

int handle_mkdir(char *args) {
    ParseArgs(MAXARGS);
    if (argc < 1) {
        ReplyNo("Usage: mkdir <dirname>");
        return 0;
    }
    char *name = argv[0];
    short mode = argc >= 2 ? (atoi(argv[1]) & 0b1111) : 0b1110;
    
    if (cmd_mkdir(name, mode) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to create file");
    }
    return 0;
}

int handle_rm(char *args) {
    ParseArgs(MAXARGS);
    if (argc < 1) {
        ReplyNo("Usage: rm <filename>");
        return 0;
    }
    char *name;
    if (cmd_rm(name) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to remove file");
    }
    return 0;
}

int handle_cd(char *args) {
    ParseArgs(MAXARGS);
    if (argc < 1) {
        ReplyNo("Usage: cd <dirname>");
        return 0;
    }
    char *ptr = NULL;
    int backup = pwd;
    if (argv[0][0] == '/') pwd = 0;  // start from root
    char *name = strtok_r(argv[0], "/", &ptr);
    while (name) {
        if (cmd_cd(name) == E_SUCCESS) {
            name = strtok_r(NULL, "/", &ptr);
            ReplyYes();
        }
        else {
            pwd = backup;
            ReplyNo("Failed to change directory");
            return 0;
        }
    }
    return 0;
}

int handle_rmdir(char *args) {
    ParseArgs(MAXARGS);
    if (argc < 1) {
        ReplyNo("Usage: rmdir <dirname>");
        return 0;
    }
    char *name = argv[0];
    if (cmd_rmdir(name) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to remove directory");
    }
    return 0;
}

int handle_ls(char *args) {
    entry *entries = NULL;
    int n = 0;
    if (cmd_ls(&entries, &n) != E_SUCCESS) {
        ReplyNo("Failed to list files");
        return 0;
    }
    ReplyYes();
    free(entries);
    return 0;
}

int handle_cat(char *args) {
    ParseArgs(MAXARGS);
    if (argc < 1) {
        ReplyNo("Usage: cat <filename>");
        return 0;
    }
    char *name = argv[0];

    uchar *buf = NULL;
    uint len;
    if (cmd_cat(name, &buf, &len) == E_SUCCESS) {
        ReplyYes();
        printf("%s\n", buf);
        free(buf);
    } else {
        ReplyNo("Failed to read file");
    }
    return 0;
}

int handle_w(char *args) {
    ParseArgs(2);
    if (argc < 2) {
        ReplyNo("Usage: w <filename> <length> <data>");
        return 0;
    }
    char *name = argv[0];
    uint len = atoi(argv[1]);
    char *data = argv[2];
    if (cmd_w(name, len, data) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to write file");
    }
    return 0;
}

int handle_i(char *args) {
    ParseArgs(3);
    if (argc < 3) {
        PrtNo("Usage: i <filename> <pos> <length> <data>");
        return 0;
    }
    char *name = argv[0];
    uint pos = atoi(argv[1]);
    uint len = atoi(argv[2]);
    char *data = argv[3];
    if (cmd_i(name, pos, len, data) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to write file");
    }
    return 0;
}

int handle_d(char *args) {
    ParseArgs(MAXARGS);
    if (argc < 3) {
        PrtNo("Usage: d <filename> <pos> <length>");
        return 0;
    }
    char *name = argv[0];
    uint pos = atoi(argv[1]);
    uint len = atoi(argv[2]);
    if (cmd_d(name, pos, len) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to delete file");
    }
    return 0;
}

// return a negative value to exit
int handle_e(char *args) {
    printf("Bye!\n");
    Log("Exit");
    return -1;
}

int handle_login(char *args) {
    int uid = atoi(args);
    if (cmd_login(uid) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to login");
    }
    return 0;
}

static struct {
    const char *name;
    int (*handler)(char *);
} cmd_table[] = {{"f", handle_f},        {"mk", handle_mk},       {"mkdir", handle_mkdir}, {"rm", handle_rm},
                 {"cd", handle_cd},      {"rmdir", handle_rmdir}, {"ls", handle_ls},       {"cat", handle_cat},
                 {"w", handle_w},        {"i", handle_i},         {"d", handle_d},         {"e", handle_e},
                 {"login", handle_login}};

#define NCMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

FILE *log_file;

int main(int argc, char *argv[]) {
    log_init("fs.log");

    assert(BSIZE % sizeof(dinode) == 0);

    // get disk info and store in global variables
    get_disk_info(&ncyl, &nsec);

    // read the superblock
    sbinit();

    static char buf[4096];
    while (1) {
        fgets(buf, sizeof(buf), stdin);
        if (feof(stdin)) break;
        buf[strlen(buf) - 1] = 0;
        Log("Use command: %s", buf);
        char *p = strtok(buf, " ");
        int ret = 1;
        for (int i = 0; i < NCMD; i++)
            if (p && strcmp(p, cmd_table[i].name) == 0) {
                ret = cmd_table[i].handler(p + strlen(p) + 1);
                break;
            }
        if (ret == 1) {
            Log("No such command");
            printf("No\n");
        }
        if (ret < 0) break;
    }

    log_close();
}
