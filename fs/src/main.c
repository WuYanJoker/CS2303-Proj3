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
    ParseArgs(1);
    if (argc < 1) {
        ReplyNo("Usage: mk <filename>");
        return 0;
    }
    char *name = argv[0];
    short mode = argc >= 2 ? (atoi(argv[1]) & 0b11111) : 0b11111;

    if (cmd_mk(name, mode) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to create file");
    }
    return 0;
}

int handle_mkdir(char *args) {
    ParseArgs(1);
    if (argc < 1) {
        ReplyNo("Usage: mkdir <dirname>");
        return 0;
    }
    char *name = argv[0];
    short mode = argc >= 2 ? (atoi(argv[1]) & 0b11111) : 0b11111;
    
    if (cmd_mkdir(name, mode) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to create file");
    }
    return 0;
}

int handle_rm(char *args) {
    ParseArgs(1);
    if (argc < 1) {
        ReplyNo("Usage: rm <filename>");
        return 0;
    }
    char *name = argv[0];
    if (cmd_rm(name) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to remove file");
    }
    return 0;
}

int handle_cd(char *args) {
    ParseArgs(1);
    if (argc < 1) {
        ReplyNo("Usage: cd <dirname>");
        return 0;
    }
    char *name = argv[0];
    if (cmd_cd(name) == E_SUCCESS) {
        ReplyYes();
    }
    else {
        ReplyNo("Failed to change directory");
    }
    return 0;
}

int handle_rmdir(char *args) {
    ParseArgs(1);
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
    char str[100];  // for time
    char logbuf[4096], *logtmp;
    printf("\33[1mType \tOwner\tUpdate time\tSize\tName\033[0m\n");
    logtmp = logbuf;
    logtmp += sprintf(logtmp, "List files\nType \tOwner\tUpdate time\tSize\tName\n");
    for (int i = 0; i < n; ++i) {
        time_t mtime = entries[i].mtime;
        struct tm *tmptr = localtime(&mtime);
        strftime(str, sizeof(str), "%m-%d %H:%M", tmptr);
        short d = entries[i].type == T_DIR;
        short m = (d << 5) | entries[i].mode;
        static char a[] = "drwrwv";
        for (int j = 0; j <= 5; j++) {
            printf("%c", m & (1 << (5 - j)) ? a[j] : '-');
            logtmp += sprintf(logtmp, "%c", m & (1 << (5 - j)) ? a[j] : '-');
        }
        printf("\t%u\t%s\t%d\t", entries[i].uid, str, entries[i].size);
        printf(d ? "\033[34m\33[1m%s\033[0m\n" : "%s\n", entries[i].name);
        // WARN: BUFFER OVERFLOW
        logtmp += sprintf(logtmp, "\t%u\t%s\t%d\t%s\n", entries[i].uid, str, entries[i].size, entries[i].name);
    }
    Log("%s", logbuf);
    ReplyYes();
    free(entries);
    return 0;
}

int handle_cat(char *args) {
    ParseArgs(1);
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
        ReplyNo("Usage: i <filename> <pos> <length> <data>");
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
    ParseArgs(3);
    if (argc < 3) {
        ReplyNo("Usage: d <filename> <pos> <length>");
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
    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s <BDSPort>>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    log_init("fs.log");

    assert(BSIZE % sizeof(dinode) == 0);

    diskseverinit(atoi(argv[1]));
    Log("Connected to disk server");

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
