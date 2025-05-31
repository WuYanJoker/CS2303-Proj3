#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "common.h"

#define MAGIC 0xFACE2025

#define NBBLOCK(size) (size / BPB + 1)

#define MAXUSER 16

typedef struct {
    ushort uid;
    uint cwd;         // current work directory
} _user;

typedef struct {
    uint magic;      // Magic number, used to identify the file system
    uint size;       // Size in blocks
    uint nblocks;
    uint ninodes;
    uint bmapstart;  // Block number of first free map block
    uint inodestart;
    uint datastart;
    uint lastmodify;
    _user users[MAXUSER]; 
    // Other fields can be added as needed
} superblock;

// Disk layout:
// superblock | bitmap | inode | data

// sb is defined in block.c
extern superblock sb;

void zero_block(uint bno);
uint allocate_block();
void free_block(uint bno);

void diskseverinit(int port);

void get_disk_info(int *ncyl, int *nsec);
void read_block(int blockno, uchar *buf);
void write_block(int blockno, uchar *buf);

#endif