#include "block.h"

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "log.h"
#include "tcp_utils.h"

superblock sb;
tcp_client diskfd;

static int _ncyl, _nsec;

void zero_block(uint bno) {
    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);
    write_block(bno, buf);
}

uint allocate_block() {
    uchar bitmap[BSIZE];
    for(int i = 0; i < sb.size; i += BPB){
        read_block(BBLOCK(i), bitmap);
        for (int j = 0; j < BPB; ++j){
            if(j == sb.size) break;
            int m = 1 << (j % 8);
            if ((bitmap[j / 8] & m) == 0) {
                bitmap[j / 8] |= m;
                write_block(BBLOCK(i), bitmap);
                zero_block(i + j);
                return i + j;
            }
        }
    }
    Warn("alloc_block: Out of blocks");
    return 0;
}

void free_block(uint bno) {
    uchar bitmap[BSIZE];
    read_block(BBLOCK(bno), bitmap);
    int i = bno % BPB;
    int m = 1 << (i % 8);
    if ((bitmap[i / 8] & m) == 0) Warn("free_block: Freeing free block");
    bitmap[i / 8] &= ~m;
    write_block(BBLOCK(bno), bitmap);
}

// #define NCYL 1024
// #define NSEC 63

// static uchar diskfile[NCYL * NSEC][BSIZE];

void diskseverinit(int port){
    diskfd = client_init("localhost", port);
}

// get disk info and store in global variables
void get_disk_info(int *ncyl, int *nsec) {
    // *ncyl = NCYL;
    // *nsec = NSEC;
    if(!diskfd){
        Error("Disk sever not found");
        return;
    }
    static char buf[1024];
    client_send(diskfd, "I", 2);
    int ret = client_recv(diskfd, buf, sizeof(buf));
    if(ret < 0){
        Warn("get_disk_info: recv error");
        return;
    }
    buf[ret] = 0;
    sscanf(buf, "%d %d", &_ncyl, &_nsec);
    *ncyl = _ncyl;
    *nsec = _nsec;
}

void read_block(int blockno, uchar *buf) {
    // memcpy(buf, diskfile[blockno], BSIZE);
    if(!diskfd){
        Error("Disk sever not found");
        return;
    }
    char msg[1024];
    sprintf(msg, "R %d %d", blockno / _nsec, blockno % _nsec);
    client_send(diskfd, msg, strlen(msg) + 1);
    memset(msg, 0, sizeof(msg));
    int ret = client_recv(diskfd, msg, sizeof(msg));
    if(ret < 0){
        Warn("read_block: recv error");
        return;
    }
    msg[ret] = 0;
    if(strcmp(msg, "No ") == 0){
        Error("write_block: error writing block");
    } else {
        memcpy(buf, msg + 4, BSIZE);  // YES -data-
    }
}

void write_block(int blockno, uchar *buf) {
    // memcpy(diskfile[blockno], buf, BSIZE);
    if(!diskfd){
        Error("Disk sever not found");
        return;
    }
    char msg[1024];
    int sptr = sprintf(msg, "W %d %d %d ", 
        blockno / _nsec, blockno % _nsec, BSIZE);
    memcpy(msg + sptr, buf, BSIZE);
    client_send(diskfd, msg, sptr + BSIZE);
    memset(msg, 0, sizeof(msg));
    int ret = client_recv(diskfd, msg, sizeof(msg));
    if(ret < 0){
        Warn("write_block: recv error");
        return;
    }
    msg[ret] = 0;
    if(strcmp(msg, "No ") == 0){
        Error("write_block: error writing block");
    }
}
