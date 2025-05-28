#include "block.h"

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "log.h"

superblock sb;
int diskfd;

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

#define NCYL 1024
#define NSEC 63

static uchar diskfile[NCYL * NSEC][512];

// get disk info and store in global variables
void get_disk_info(int *ncyl, int *nsec) {
    *ncyl = NCYL;
    *nsec = NSEC;
}

void read_block(int blockno, uchar *buf) {
    memcpy(buf, diskfile[blockno], 512);
}

void write_block(int blockno, uchar *buf) {
    memcpy(diskfile[blockno], buf, 512);
}
