#include "inode.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "block.h"
#include "log.h"

inode *iget(uint inum) {
    if (inum < 0 || inum >= sb.ninodes) {
        Warn("iget: Invaild inum %d", inum);
        return NULL;
    }
    uchar buf[BSIZE];
    read_block(IBLOCK(inum), buf);
    dinode *dip = (dinode *)buf + inum % IPB;
    if (dip->type == 0) {
        Warn("iget: Inode %d not exist", inum);
        return NULL;
    }
    inode *ip = calloc(1, sizeof(inode));
    ip->inum = inum;
    ip->type = dip->type;
    ip->mode = dip->mode;
    ip->uid = dip->uid;
    ip->links = dip->links;
    ip->mtime = dip->mtime;
    ip->size = dip->size;
    ip->blocks = dip->blocks;
    memcpy(ip->addrs, dip->addrs, sizeof(ip->addrs));
    return ip;
}

void iput(inode *ip) { free(ip); }

inode *ialloc(short type) {
    uchar buf[BSIZE];
    for(int i = 0; i < sb.ninodes; ++i){
        read_block(IBLOCK(i), buf);
        dinode *dip = (dinode *)buf + i % IPB;
        if (dip->type == 0) {
            memset(dip, 0, sizeof(dinode));
            dip->type = type;
            write_block(IBLOCK(i), buf);
            inode *ip = calloc(1, sizeof(inode));
            ip->inum = i;
            ip->type = type;
            return ip;
        }
    }
    Error("ialloc: no free inodes");
    return NULL;
}

void iupdate(inode *ip) {
    uchar buf[BSIZE];
    read_block(IBLOCK(ip->inum), buf);
    dinode *dip = (dinode *)buf + ip->inum % IPB;
    dip->type = ip->type;
    dip->mode = ip->mode;
    dip->uid = ip->uid;
    dip->links = ip->links;
    dip->mtime = ip->mtime = time(NULL);
    dip->size = ip->size;
    dip->blocks = ip->blocks;
    memcpy(dip->addrs, ip->addrs, sizeof(ip->addrs));
    write_block(IBLOCK(ip->inum), buf);
}

int readi(inode *ip, uchar *dst, uint off, uint n) {
    uchar buf[BSIZE];
    if (off > ip->size || off + n < off) return -1;
    if (off + n > ip->size) n = ip->size - off;

    for (uint tot = 0, m; tot < n; tot += m, off += m, dst += m) {
        read_block(imapblock(ip, off / BSIZE), buf);
        m = min(n - tot, BSIZE - off % BSIZE);
        memcpy(dst, buf + off % BSIZE, m);
    }
    return n;
}

int writei(inode *ip, uchar *src, uint off, uint n) {
    uchar buf[BSIZE];
    if (off > ip->size || off + n < off)  // off is larger than size || off overflow
        return -1;
    if (off + n > MAXFILEB * BSIZE) return -1;  // too large

    for (uint tot = 0, m; tot < n; tot += m, off += m, src += m) {
        read_block(imapblock(ip, off / BSIZE), buf);
        m = min(n - tot, BSIZE - off % BSIZE);
        memcpy(buf + off % BSIZE, src, m);
        write_block(imapblock(ip, off / BSIZE), buf);
    }

    if (n > 0 && off > ip->size) {  // size is larger
        ip->size = off;
        ip->blocks = max(1 + (off - 1) / BSIZE, ip->blocks);  // blocks may change
    }
    ip->mtime = time(NULL);
    iupdate(ip);
    return n;
}

int checkIp(inode *ip){
    if(!ip){
        Warn("Inode not exist");
    }
    return ip == NULL;
}

int itest(inode *ip) {
    int true_blocks = 1 + (ip->size - 1) / BSIZE;
    if (true_blocks <= ip->blocks / 2) {
        Log("Block usage: %d/%d, recycle", true_blocks, ip->blocks);
        for(int i = ip->blocks - 1; i > true_blocks; --i) free_block(imapblock(ip, i));
        ip->blocks = true_blocks;
        iupdate(ip);
    }
    return 0;
}

int icreate(short type, char *name, uint pinum, ushort uid, ushort perm) {
    inode *ip = ialloc(type);
    checkIp(ip);
    ip->mode = perm;
    ip->uid = uid;
    ip->links = 1;
    ip->size = 0;
    ip->blocks = 0;
    uint inum = ip->inum;
    if (type == T_DIR) {
        dirent des[2];
        des[0].inum = inum;
        strcpy(des[0].name, ".");
        des[1].inum = pinum;
        strcpy(des[1].name, "..");
        writei(ip, (uchar *)&des, ip->size, sizeof(des));
    }
    else iupdate(ip);
    Log("Create %s inode %d, inside directory inode %d",
        type == T_DIR ? "dir" : "file", ip->inum, pinum);

    iput(ip);
    if (pinum != inum) {  // root will not enter here
                          // for normal files, add it to the parent directory
        ip = iget(pinum);
        checkIp(ip);
        dirent dir;
        dir.inum = inum;
        strcpy(dir.name, name);
        writei(ip, (uchar *)&dir, ip->size, sizeof(dir));
        iput(ip);
    }
    return 0;
}

void itrunc(inode *ip) {
    uchar buf[BSIZE];
    for(int i = 0; i < NDIRECT; ++i)
        if (ip->addrs[i]) {
            free_block(ip->addrs[i]);
            ip->addrs[i] = 0;
        }

    if (ip->addrs[NDIRECT]) {
        read_block(ip->addrs[NDIRECT], buf);
        uint *addrs = (uint *)buf;
        for(int i = 0; i < APB; ++i)
            if(addrs[i]) free_block(addrs[i]);
        free_block(ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }

    if (ip->addrs[NDIRECT + 1]) {
        read_block(ip->addrs[NDIRECT + 1], buf);
        uint *addrs = (uint *)buf;
        uchar buf2[BSIZE];
        for(int i = 0; i < APB; ++i) {
            if(addrs[i]) {
                read_block(addrs[i], buf2);
                uint *addrs2 = (uint *)buf2;
                for(int j = 0; j < APB; ++j)
                    if (addrs2[j]) free_block(addrs2[j]);
                free_block(addrs[i]);
            }
        }
        free_block(ip->addrs[NDIRECT + 1]);
        ip->addrs[NDIRECT + 1] = 0;
    }

    ip->size = 0;
    ip->blocks = 0;
    iupdate(ip);
}

int imapblock(inode *ip, uint bno) {
    uchar buf[BSIZE];
    uint addr;
    if (bno < NDIRECT) {
        addr = ip->addrs[bno];
        if (!addr) addr = ip->addrs[bno] = allocate_block();
        return addr;
    } else if (bno < NDIRECT + APB) {
        bno -= NDIRECT;
        uint saddr = ip->addrs[NDIRECT];  // single addr
        if (!saddr) saddr = ip->addrs[NDIRECT] = allocate_block();
        read_block(saddr, buf);
        uint *addrs = (uint *)buf;
        addr = addrs[bno];
        if (!addr) {
            addr = addrs[bno] = allocate_block();
            write_block(saddr, buf);
        }
        return addr;
    } else if (bno < MAXFILEB) {
        bno -= NDIRECT + APB;
        uint a = bno / APB, b = bno % APB;
        uint daddr = ip->addrs[NDIRECT + 1];  // double addr
        if (!daddr) daddr = ip->addrs[NDIRECT + 1] = allocate_block();
        read_block(daddr, buf);
        uint *addrs = (uint *)buf;

        uint saddr = addrs[a];  // single addr
        if (!saddr) {
            saddr = addrs[a] = allocate_block();
            write_block(daddr, buf);
        }
        read_block(saddr, buf);
        addrs = (uint *)buf;

        addr = addrs[b];
        if (!addr) {
            addr = addrs[b] = allocate_block();
            write_block(saddr, buf);
        }
        return addr;
    } else {
        Warn("imapblock: bno too large");
        return 0;
    }
}
