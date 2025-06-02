#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "block.h"
#include "common.h"
#include "fs.h"
#include "log.h"
#include "tcp_utils.h"

// global variables
int ncyl, nsec;
id_map clientmap[MAXUSER];
pthread_mutex_t  mutex_lock;

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

int backInfo(char *buf, int ret){
    switch (ret)
    {
    case E_NOT_LOGGED_IN:
        sprintf(buf, "Please enter your UID: login <uid>");
        break;
    case E_NOT_FORMATTED:
        sprintf(buf, "Not formatted");
        break;
    case E_PERMISSION_DENIED:
        sprintf(buf, "Permisson denied");
        break;
    case E_SUCCESS:
    case E_ERROR:
    default:
        break;
    }
    return ret;
}

int handle_f(tcp_buffer *wb, char *args, int len) {
    // f
    char buf[64];
    int ret = backInfo(buf, cmd_f(ncyl, nsec));
    if (ret == E_SUCCESS) {
        reply_with_yes(wb, NULL, 0);
    } else {
        if(ret == E_ERROR) sprintf(buf, "Failed to format");
        else if(ret == E_PERMISSION_DENIED) sprintf(buf, "Only UID=1 is authorized to format");
        reply_with_no(wb, buf, strlen(buf) + 1);
    }
    return 0;
}

int handle_mk(tcp_buffer *wb, char *args, int len) {
    // mk f (access)
    char buf[64];
    ParseArgs(2);
    if (argc < 1) {
        sprintf(buf, "Usage: mk <filename>");
        reply_with_no(wb, buf, strlen(buf) + 1);
        return 0;
    }
    char *name = argv[0];
    short mode = argc >= 2 ? (atoi(argv[1]) & 0b11111) : 0b11111;

    int ret = backInfo(buf, cmd_mk(name, mode));
    if (ret == E_SUCCESS) {
        reply_with_yes(wb, NULL, 0);
        Log("mk %s %d", name, mode);
    } else {
        if(ret == E_ERROR) sprintf(buf, "Failed to create file");
        reply_with_no(wb, buf, strlen(buf) + 1);
    }
    return 0;
}

int handle_mkdir(tcp_buffer *wb, char *args, int len) {
    // mkdir d (access)
    char buf[64];
    ParseArgs(2);
    if (argc < 1) {
        sprintf(buf, "Usage: mkdir <dirname>");
        reply_with_no(wb, buf, strlen(buf) + 1);
        return 0;
    }
    char *name = argv[0];
    short mode = argc >= 2 ? (atoi(argv[1]) & 0b11111) : 0b11111;
    
    int ret = backInfo(buf, cmd_mkdir(name, mode));
    if (ret == E_SUCCESS) {
        reply_with_yes(wb, NULL, 0);
        Log("mkdir %s %d", name, mode);
    } else {
        if(ret == E_ERROR) sprintf(buf, "Failed to create directory");
        reply_with_no(wb, buf, strlen(buf) + 1);
    }
    return 0;
}

int handle_rm(tcp_buffer *wb, char *args, int len) {
    // rm f
    char buf[64];
    ParseArgs(1);
    if (argc < 1) {
        sprintf(buf, "Usage: rm <filename>");
        reply_with_no(wb, buf, strlen(buf) + 1);
        return 0;
    }
    char *name = argv[0];

    int ret = backInfo(buf, cmd_rm(name));
    if (ret == E_SUCCESS) {
        reply_with_yes(wb, NULL, 0);
    } else {
        if(ret == E_ERROR) sprintf(buf, "Failed to remove file");
        reply_with_no(wb, buf, strlen(buf) + 1);
    }
    return 0;
}

int handle_cd(tcp_buffer *wb, char *args, int len) {
    // cd path
    char buf[64];
    ParseArgs(1);
    if (argc < 1) {
        sprintf(buf, "Usage: cd <dirname>");
        reply_with_no(wb, buf, strlen(buf) + 1);
        return 0;
    }
    char *name = argv[0];

    int ret = backInfo(buf, cmd_cd(name));
    if (ret == E_SUCCESS) {
        reply_with_yes(wb, NULL, 0);
    } else {
        if(ret == E_ERROR) sprintf(buf, "Failed to change directory");
        reply_with_no(wb, buf, strlen(buf) + 1);
    }
    return 0;
}

int handle_rmdir(tcp_buffer *wb, char *args, int len) {
    // rmdir d
    char buf[64];
    ParseArgs(1);
    if (argc < 1) {
        sprintf(buf, "Usage: rmdir <dirname>");
        reply_with_no(wb, buf, strlen(buf) + 1);
        return 0;
    }
    char *name = argv[0];

    int ret = backInfo(buf, cmd_rmdir(name));
    if (ret == E_SUCCESS) {
        reply_with_yes(wb, NULL, 0);
    } else {
        if(ret == E_ERROR) sprintf(buf, "Failed to remove directory");
        reply_with_no(wb, buf, strlen(buf) + 1);
    }
    return 0;
}

int handle_ls(tcp_buffer *wb, char *args, int len) {
    // ls
    char buf[4096];
    entry *entries = NULL;
    int n = 0;
    int ret = backInfo(buf, cmd_ls(&entries, &n));
    if (ret == E_SUCCESS) {
        char str[100];  // for time
        sprintf(buf, "\33[1mType \tOwner\tUpdate time\tSize\tName\033[0m\n");
        for (int i = 0; i < n; ++i) {
            time_t mtime = entries[i].mtime;
            struct tm *tmptr = localtime(&mtime);
            strftime(str, sizeof(str), "%m-%d %H:%M", tmptr);
            short d = entries[i].type == T_DIR;
            short m = (d << 5) | entries[i].mode;
            static char a[] = "drwrwv";
            for (int j = 0; j <= 5; ++j) {
                sprintf(buf + strlen(buf), "%c", m & (1 << (5 - j)) ? a[j] : '-');
            }
            sprintf(buf + strlen(buf), "\t%u\t%s\t%d\t", entries[i].uid, str, entries[i].size);
            sprintf(buf + strlen(buf), d ? "\033[34m\33[1m%s\033[0m\n" : "%s\n", entries[i].name);
        }
        // reply_with_yes(wb, NULL, 0);
        reply(wb, buf, strlen(buf) + 1);
    } else {
        if(ret == E_ERROR) sprintf(buf, "Failed to list files");
        reply_with_no(wb, buf, strlen(buf) + 1);
    }
    free(entries);
    return 0;
}

int handle_cat(tcp_buffer *wb, char *args, int len) {
    // cat f
    char buf[64];
    ParseArgs(1);
    if (argc < 1) {
        sprintf(buf, "Usage: cat <filename>");
        reply_with_no(wb, buf, strlen(buf) + 1);
        return 0;
    }
    char *name = argv[0];

    uchar *databuf = NULL;
    uint datalen;
    int ret = backInfo(buf, cmd_cat(name, &databuf, &datalen));
    if (ret == E_SUCCESS) {
        reply_with_yes(wb, (const char *)databuf, datalen + 1);
        free(databuf);
    } else {
        if(ret == E_ERROR) sprintf(buf, "Failed to read file");
        reply_with_no(wb, buf, strlen(buf) + 1);
    }
    return 0;
}

int handle_w(tcp_buffer *wb, char *args, int len) {
    // w f l data
    char buf[64];
    ParseArgs(2);
    if (argc < 2) {
        sprintf(buf, "Usage: w <filename> <length> <data>");
        reply_with_no(wb, buf, strlen(buf) + 1);
        return 0;
    }
    char *name = argv[0];
    uint datalen = atoi(argv[1]);
    char *data = argv[2];

    int ret = backInfo(buf, cmd_w(name, datalen, data));
    if (ret == E_SUCCESS) {
        reply_with_yes(wb, NULL, 0);
    } else {
        if(ret == E_ERROR) sprintf(buf, "Failed to write file");
        reply_with_no(wb, buf, strlen(buf) + 1);
    }
    return 0;
}

int handle_i(tcp_buffer *wb, char *args, int len) {
    // i f pos l data
    char buf[64];
    ParseArgs(3);
    if (argc < 3) {
        sprintf(buf, "Usage: i <filename> <pos> <length> <data>");
        reply_with_no(wb, buf, strlen(buf) + 1);
        return 0;
    }
    char *name = argv[0];
    uint pos = atoi(argv[1]);
    uint datalen = atoi(argv[2]);
    char *data = argv[3];

    int ret = backInfo(buf, cmd_i(name, pos, datalen, data));
    if (ret == E_SUCCESS) {
        reply_with_yes(wb, NULL, 0);
    } else {
        if(ret == E_ERROR) sprintf(buf, "Failed to insert file");
        reply_with_no(wb, buf, strlen(buf) + 1);
    }
    return 0;
}

int handle_d(tcp_buffer *wb, char *args, int len) {
    // d f pos l
    char buf[64];
    ParseArgs(3);
    if (argc < 3) {
        sprintf(buf, "Usage: d <filename> <pos> <length>");
        reply_with_no(wb, buf, strlen(buf) + 1);
        return 0;
    }
    char *name = argv[0];
    uint pos = atoi(argv[1]);
    uint datalen = atoi(argv[2]);

    int ret = backInfo(buf, cmd_d(name, pos, datalen));
    if (ret == E_SUCCESS) {
        reply_with_yes(wb, NULL, 0);
    } else {
        if(ret == E_ERROR) sprintf(buf, "Failed to delete file");
        reply_with_no(wb, buf, strlen(buf) + 1);
    }
    return 0;
}

// return a negative value to exit
int handle_e(tcp_buffer *wb, char *args, int len) {
    // e
    const char *msg = "Bye!";
    reply(wb, msg, strlen(msg) + 1);
    Log("Exit");
    cmd_exit();
    return -1;
}

int handle_login(tcp_buffer *wb, char *args, int len) {
    // login <uid>
    char msg[64];
    int uid = atoi(args);
    if (cmd_login(uid) == E_SUCCESS) {
        Log("login: uid=%u", uid);
        sprintf(msg, "Hello, uid=%u!", uid);
        reply_with_yes(wb, msg, strlen(msg) + 1);
    } else {
        sprintf(msg, "Failed to login");
        reply_with_no(wb, msg, strlen(msg) + 1);
    }
    return 0;
}

int handle_pwd(tcp_buffer *wb, char *args, int len) {
    // pwd
    char buf[64];
    uint msglen;
    char *msg = NULL;
    int ret = backInfo(buf, cmd_pwd(&msg, &msglen));
    if(ret == E_SUCCESS){
        Log("pwd: %s", msg);
        reply_with_yes(wb, msg, msglen + 1);
        free(msg);
    }
    else{
        if(ret == E_ERROR) sprintf(buf, "Failed to show pwd");
        reply_with_no(wb, buf, strlen(buf) + 1);
    }
    return 0;
}

static struct {
    const char *name;
    int (*handler)(tcp_buffer *wb, char *, int);
} cmd_table[] = {{"f", handle_f},        {"mk", handle_mk},       {"mkdir", handle_mkdir}, {"rm", handle_rm},
                 {"cd", handle_cd},      {"rmdir", handle_rmdir}, {"ls", handle_ls},       {"cat", handle_cat},
                 {"w", handle_w},        {"i", handle_i},         {"d", handle_d},         {"e", handle_e},
                 {"login", handle_login},{"pwd", handle_pwd}};

#define NCMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

void on_connection(int id) {
    // const char *msg = "Welcome to my file system!\nTap 'login <uid>' to login.";
    for(int i = 0; i < MAXUSER; ++i){
        if(clientmap[i].client_id == -1){
            clientmap[i].client_id = id;
            Log("Create map:fd %d in %d", id, i);
            return;
        }
    }
    Error("No free uid space");
}

int on_recv(int id, tcp_buffer *wb, char *msg, int len) {
    char *p = strtok(msg, " \r\n");
    Log("Use command: %s", p);
    int ret = 1;
    for (int i = 0; i < NCMD; ++i)
        if (p && strcmp(p, cmd_table[i].name) == 0) {
            pthread_mutex_lock(&mutex_lock);
            load_user(clientmap, id);
            ret = cmd_table[i].handler(wb, p + strlen(p) + 1, len - strlen(p) - 1);
            save_user(clientmap, id);
            pthread_mutex_unlock(&mutex_lock);
            break;
        }
    if (ret == 1) {
        static char unk[] = "Unknown command";
        buffer_append(wb, unk, sizeof(unk));
    }
    if (ret < 0) {
        return -1;
    }
    return 0;
}

void cleanup(int id) {
    // some code that are executed when a client is disconnected 
    for(int i = 0; i < MAXUSER; ++i){
        if(clientmap[i].client_id == id){
            clientmap[i].client_id = -1;
        }
        break;
    }  
}

FILE *log_file;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s <BDSPort> <FSPort>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    log_init("fs.log");

    assert(BSIZE % sizeof(dinode) == 0);
    assert(BSIZE % sizeof(dirent) == 0);

    diskseverinit(atoi(argv[1]));
    Log("Connected to disk server");

    // get disk info and store in global variables
    get_disk_info(&ncyl, &nsec);
    Log("ncyl=%d, nsec=%d", ncyl, nsec);

    // read the superblock
    sbinit();
    Log("Superblock initialized, %sformatted", sb.magic == MAGIC ? "" : "not ");
    Log("size=%u, nblocks=%u, ninodes=%u", sb.size, sb.nblocks, sb.ninodes);

    pthread_mutex_init(&mutex_lock, NULL);
    for(int i = 0; i < MAXUSER; ++i) clientmap[i].client_id = -1;
    // command
    tcp_server server = server_init(atoi(argv[2]), 12, on_connection, on_recv, cleanup);
    server_run(server);

    // never reached
    log_close();
}
