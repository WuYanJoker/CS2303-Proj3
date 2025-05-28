#ifndef __INODE_H__
#define __INODE_H__

#include "common.h"

#define NDIRECT 9  // Direct blocks, you can change this value

#define MAXFILEB (NDIRECT + APB + APB * APB)

#define NINODE(size) (size / 2)
#define NIBLOCK(size) (size / 2 / IPB + 1)

enum {
    T_DIR = 1,   // Directory
    T_FILE = 2,  // File
};

// You should add more fields
// the size of a dinode must divide BSIZE
typedef struct {  // 64 bytes
    ushort type;              // File type
    ushort mode;
    ushort uid;
    ushort links;
    uint mtime;
    uint size;                // Size in bytes
    uint blocks;              // Number of blocks, may be larger than size
    uint addrs[NDIRECT + 2];  // Data block addresses, the last two are indirect blocks
    // ...
    // ...
    // Other fields can be added as needed
} dinode;

// inode in memory
// more useful fields can be added, e.g. reference count
typedef struct {
    uint inum;
    ushort type;
    ushort mode;
    ushort uid;
    ushort links;
    uint mtime;
    uint size;
    uint blocks;
    uint addrs[NDIRECT + 2]; // the last two are indirect blocks
    // ...
    // ...
    // Other fields can be added as needed
} inode;

// You can change the size of MAXNAME
#define MAXNAME 12

typedef struct {  // 16 bytes
    uint inum;
    char name[MAXNAME];
} dirent;

// Get an inode by number (returns allocated inode or NULL)
// Don't forget to use iput()
inode *iget(uint inum);

// Free an inode (or decrement reference count)
void iput(inode *ip);

// Allocate a new inode of specified type (returns allocated inode or NULL)
// Don't forget to use iput()
inode *ialloc(short type);

// Update disk inode with memory inode contents
void iupdate(inode *ip);

// Read from an inode (returns bytes read or -1 on error)
int readi(inode *ip, uchar *dst, uint off, uint n);

// Write to an inode (returns bytes written or -1 on error)
int writei(inode *ip, uchar *src, uint off, uint n);

// check inode pointer
void checkIp(inode *ip);

// test if ip->blocks is too larger than size, recycle blocks
int itest(inode *ip);

// Create an inode, return 0 when success
int icreate(short type, char *name, uint pinum, ushort uid, ushort perm);

// free all data blocks of an inode, but not the inode itself
void itrunc(inode *ip);

// if not exists, alloc a block for ip->addrs bn
int imapblock(inode *ip, uint bno);

#endif
