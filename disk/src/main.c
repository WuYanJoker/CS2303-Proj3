#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "disk.h"
#include "log.h"

#define ParseArgs(maxargs)          \
    char *argv[maxargs + 1];        \
    int argc = parse(args, argv, maxargs);

int parse(char *line, char *argv[], int lim) {
    char *p;
    int argc = 0;
    p = strtok(line, " ");
    while (p) {
        argv[argc++] = p;
        if (argc >= lim) break;
        p = strtok(NULL, " ");
    }
    if (argc >= lim) {
        argv[argc] = p + strlen(p) + 1;
    } else {
        argv[argc] = NULL;
    }
    return argc;
}

// return a negative value to exit the program
int handle_i(char *args) {
    int ncyl, nsec;
    cmd_i(&ncyl, &nsec);
    printf("%d %d\n", ncyl, nsec);
    return 0;
}

int handle_r(char *args) {
    // Parse the arguments
    ParseArgs(MAXARGS);
    if (argc < 2) {
        printf("No\n");
        Log("Invalid arguments");
        return 0;
    }
    int cyl = atoi(argv[0]);
    int sec = atoi(argv[1]);
    char buf[512];

    // Call the cmd_r function
    if (cmd_r(cyl, sec, buf) == 0) {
        printf("Yes\n");
        for (int i = 0; i < 512; i++) {
            printf("%c", buf[i]);
        }
        printf("\n");
    } else {
        printf("No\n");
    }
    return 0;
}

int handle_w(char *args) {
    ParseArgs(MAXARGS);
    if (argc < 3) {
        printf("No\n");
        Log("Invalid arguments");
        return 0;
    }
    int cyl = atoi(argv[0]);
    int sec = atoi(argv[1]);
    char *data = argv[2];
    int len = strlen(data);

    if (cmd_w(cyl, sec, len, data) == 0) {
        printf("Yes\n");
    } else {
        printf("No\n");
    }
    return 0;
}

int handle_e(char *args) {
    printf("Bye!\n");
    Log("Exit disk");
    return -1;
}

static struct {
    const char *name;
    int (*handler)(char *);
} cmd_table[] = {
    {"I", handle_i},
    {"R", handle_r},
    {"W", handle_w},
    {"E", handle_e},
};

#define NCMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

FILE *log_file;

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr,
                "Usage: %s <disk file name> <cylinders> <sector per cylinder> "
                "<track-to-track delay>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    // args
    char *filename = argv[1];
    int ncyl = atoi(argv[2]);
    int nsec = atoi(argv[3]);
    int ttd = atoi(argv[4]);  // ms

    log_init("disk.log");

    int ret = init_disk(filename, ncyl, nsec, ttd);
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize disk\n");
        exit(EXIT_FAILURE);
    }

    // command
    static char buf[1024];
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

    close_disk();
    log_close();
}
