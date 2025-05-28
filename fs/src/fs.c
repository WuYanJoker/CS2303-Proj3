#include "fs.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "block.h"
#include "log.h"

ushort uid;
uint pwd;

void sbinit() {
    uchar buf[BSIZE];
    read_block(0, buf);
    memcpy(&sb, buf, sizeof(sb));
}

int format(int *ncyl, int *nsec){
    uchar buf[BSIZE];

    sb.magic = MAGIC;
    sb.size = (*ncyl) * (*nsec);

    int ninodes = NINODE(sb.size);
    int nallcatedb = 1 + NBBLOCK(sb.size) + NIBLOCK(sb.size);

    sb.ninodes = ninodes;
    sb.nblocks = sb.size - 1 - NBBLOCK(sb.size) - NIBLOCK(sb.size);
    sb.bmapstart = 1;
    sb.inodestart = 1 + NIBLOCK(sb.size);

    // zero superblock
    memset(buf, 0, BSIZE);
    memcpy(buf, &sb, sizeof(sb));
    write_block(0, buf);

    // bitmap init
    memset(buf, 0, BSIZE);
    for (int i = 0; i < sb.size; i += BPB) write_block(BBLOCK(i), buf);
    for (int i = 0; i < ninodes; i += IPB) write_block(IBLOCK(i), buf);

    // mark bitmap and inode blocks as in use
    for (int i = 0; i < nallcatedb; i += BPB) {
        memset(buf, 0, BSIZE);
        for (int j = 0; j < BPB; ++j)
            if (i + j < nallcatedb) buf[j / 8] |= 1 << (j % 8);
        write_block(BBLOCK(i), buf);
    }

    pwd = 0;
    // make root dir
    if (!icreate(T_DIR, NULL, 0, 0, 0b1111)){
        Log("Disk format done");
    }
    else return -1;
    return sb.size;
}

int checkLogin(){
    if(uid == 0){
        Warn("Please enter your UID: login <uid>\n");
        return E_NOT_LOGGED_IN;
    }
    return E_SUCCESS;
}

int checkFmt(){
    int ret = checkLogin();
    if(sb.magic != MAGIC){
        Error("Not formatted");
        return E_NOT_FORMATTED;
    }
    return ret ? E_NOT_LOGGED_IN : E_SUCCESS;
}

int checkPermission(uint inum, short perm){
    inode *ip = iget(inum);
    checkIp(ip);
    int ret = 0;
    if (ip->uid == uid)
        ret = 1;
    else {
        if ((ip->mode & perm) == perm)
            ret = 1;
        else{
            ret = 0;
            Warn("Permisson denied");
        }
    }
    iput(ip);
    return ret ? E_SUCCESS : E_PERMISSION_DENIED;
}

// if name is valid for file or dir
int is_name_valid(char *name) {
    int len = strlen(name);
    if (len >= MAXNAME) return 0;
    if (name[0] == '.') return 0;
    if (strcmp(name, "/") == 0) return 0;
    // static char invalid[] = "\\/<>?\":| @#$&();*";
    // int invalidlen = strlen(invalid);
    // for (int i = 0; i < len; i++)
    //     for (int j = 0; j < invalidlen; j++)
    //         if (name[i] == invalid[j]) return 0;
    return 1;
}

// find inum from pwd
uint findinum(char *name) {
    inode *ip = iget(pwd);
    checkIp(ip);
    
    uchar *buf = malloc(ip->size);
    readi(ip, buf, 0, ip->size);
    dirent *dir = (dirent *)buf;

    int ninodes = NINODE(sb.size);
    int result = ninodes;
    int nfile = ip->size / sizeof(dirent);
    for (int i = 0; i < nfile; i++) {
        if (dir[i].inum == ninodes) continue;
        if (strcmp(dir[i].name, name) == 0) {
            result = dir[i].inum;
            break;
        }
    }
    free(buf);
    iput(ip);
    return result;
}

// delete inode from pwd
int delinum(uint inum) {
    inode *ip = iget(pwd);
    checkIp(ip);

    uchar *buf = malloc(ip->size);
    readi(ip, buf, 0, ip->size);
    dirent *dir = (dirent *)buf;

    int ninodes = NINODE(sb.size);
    int nfile = ip->size / sizeof(dirent);
    int deleted = 1;
    for (int i = 0; i < nfile; i++) {
        if (dir[i].inum == ninodes) {
            ++deleted;
            continue;
        }
        if (dir[i].inum == inum) {
            dir[i].inum = ninodes;
            writei(ip, (uchar *)&dir[i], i * sizeof(dirent),
                   sizeof(dirent));
        }
    }

    if (deleted > nfile / 2) {
        int newn = nfile - deleted, newsize = newn * sizeof(dirent);
        uchar *newbuf = malloc(newsize);
        dirent *newde = (dirent *)newbuf;
        int j = 0;
        for (int i = 0; i < nfile; i++) {
            if (dir[i].inum == ninodes) continue;
            memcpy(&newde[j++], &dir[i], sizeof(dirent));
        }
        assert(j == newn);
        ip->size = newsize;
        writei(ip, newbuf, 0, newsize);
        free(newbuf);
        itest(ip);  // try to shrink
    }

    free(buf);
    iput(ip);
    return 0;
}

int cmd_f(int ncyl, int nsec) {
    if(ncyl <= 0 || nsec <= 0){
        return E_ERROR;
    }
    int nformat = format(&ncyl, &nsec);
    if(nformat < 0){
        return E_ERROR;
    }
    return E_SUCCESS;
}

int cmd_mk(char *name, short mode) {
    int ret = checkFmt();
    if(ret) return ret;
    if (!is_name_valid(name)) {
        Warn("mk: Invalid name!");
        return E_ERROR;
    }
    if (findinum(name) != NINODE(sb.size)) {
        Warn("mk: %s already exists!", name);
        return E_ERROR;
    }
    ret = checkPermission(pwd, R | W);
    if(ret) return ret;
    if(icreate(T_FILE, name, pwd, uid, mode)) return E_ERROR;
    return E_SUCCESS;
}

int cmd_mkdir(char *name, short mode) {
    int ret = checkFmt();
    if(ret) return ret;
    if (!is_name_valid(name)) {
        Warn("mkdir: Invalid name!");
        return E_ERROR;
    }
    if (findinum(name) != NINODE(sb.size)) {
        Warn("mkdir: %s already exists!", name);
        return E_ERROR;
    }
    ret = checkPermission(pwd, R | W);
    if(ret) return ret;
    if(icreate(T_DIR, name, pwd, uid, mode)) return E_ERROR;
    return E_SUCCESS;
}

int cmd_rm(char *name) {
    int ret = checkFmt();
    if(ret) return ret;
    uint inum = findinum(name);
    if (inum == NINODE(sb.size)) {
        return E_ERROR;
    }
    ret = checkPermission(inum, W);
    ret |= checkPermission(pwd, R | W);
    if(ret) return ret;
    inode *ip = iget(inum);
    checkIp(ip);
    if (ip->type != T_FILE) {
        Warn("rm: Not a file, please use rmdir");
        iput(ip);
        return E_ERROR;
    }
    if (--ip->links == 0) {
        itrunc(ip);
    } else {
        iupdate(ip);
    }
    iput(ip);

    delinum(inum);
    return E_SUCCESS;
}

int cmd_rmdir(char *name) {
    int ninodes = NINODE(sb.size);
    int ret = checkFmt();
    if(ret) return ret;
    uint inum = findinum(name);
    if (inum == ninodes) {
        Warn("rmdir: Not found!");
        return E_ERROR;
    }
    ret = checkPermission(inum, R | W);
    ret |= checkPermission(pwd, R | W);
    if(ret) return ret;
    inode *ip = iget(inum);
    checkIp(ip);
    if (ip->type != T_DIR) {
        Warn("rmdir: Not a directory, please use rm");
        iput(ip);
        return E_ERROR;
    }

    // if dir is not empty
    int empty = 1;
    uchar *buf = malloc(ip->size);
    readi(ip, buf, 0, ip->size);
    dirent *dir = (dirent *)buf;

    int nfile = ip->size / sizeof(dirent);
    for (int i = 0; i < nfile; ++i) {
        if (dir[i].inum == ninodes) continue;  // deleted
        if (strcmp(dir[i].name, ".") == 0 || strcmp(dir[i].name, "..") == 0)
            continue;
        empty = 0;
        break;
    }
    free(buf);

    if (!empty) {
        Warn("rmdir: Directory not empty!");
        iput(ip);
        return E_ERROR;
    }

    // ok, delete
    itrunc(ip);
    iput(ip);
    delinum(inum);
    return E_SUCCESS;
}

int cmd_cd(char *name) {
    int ret = checkFmt();
    if(ret) return ret;
    uint inum = findinum(name);
    if (inum == NINODE(sb.size)) {
        Warn("cd: Not found!");
        return E_ERROR;
    }
    ret = checkPermission(inum, R);
    if(ret) return ret;
    inode *ip = iget(inum);
    checkIp(ip);
    if (ip->type != T_DIR) {
        Warn("cd: Not a directory");
        iput(ip);
        return E_ERROR;
    }
    pwd = inum;
    iput(ip);
    return E_SUCCESS;
}

int cmp_ls(const void *a, const void *b) {
    entry *da = (entry *)a;
    entry *db = (entry *)b;
    if (da->type != db->type) {
        if (da->type == T_DIR)
            return -1;  // dir first
        else
            return 1;
    }
    // same type, compare name
    return strcmp(da->name, db->name);
}

int cmd_ls(entry **entries, int *n) {
    int ret = checkFmt();
    ret |= checkPermission(pwd, R);
    if(ret) return ret;
    inode *ip = iget(pwd);
    checkIp(ip);

    uchar *buf = malloc(ip->size);
    readi(ip, buf, 0, ip->size);
    dirent *dir = (dirent *)buf;

    int nfile = ip->size / sizeof(dirent), ninodes = NINODE(sb.size);
    *n = 0;
    if(*entries != NULL){
        Warn("ls: Dirty entry");
        // return E_ERROR;
    }
    *entries = malloc(nfile * sizeof(entry));
    for(int i = 0; i < nfile; ++i) {
        if (dir[i].inum == ninodes) continue;  // deleted
        if (strcmp(dir[i].name, ".") == 0 || strcmp(dir[i].name, "..") == 0)
            continue;
        inode *sub = iget(dir[i].inum);
        checkIp(sub);
        (*entries)[*n].type = sub->type;
        strcpy((*entries)[*n].name, dir[i].name);
        (*entries)[*n].mtime = sub->mtime;
        (*entries)[*n].uid = sub->uid;
        (*entries)[*n].mode = sub->mode;
        (*entries)[*n].size = sub->size;
        ++*n;
        iput(sub);
    }
    qsort(*entries, *n, sizeof(entry), cmp_ls);
    static char str[100];  // for time
    static char logbuf[4096], *logtmp;
    printf("\33[1mType \tOwner\tUpdate time\tSize\tName\033[0m\n");
    logtmp = logbuf;
    logtmp += sprintf(logtmp, "List files\nType \tOwner\tUpdate time\tSize\tName\n");
    for (int i = 0; i < *n; ++i) {
        time_t mtime = (*entries)[i].mtime;
        struct tm *tmptr = localtime(&mtime);
        strftime(str, sizeof(str), "%m-%d %H:%M", tmptr);
        short d = (*entries)[i].type == T_DIR;
        short m = (d << 4) | (*entries)[i].mode;
        static char a[] = "drwrw";
        for (int j = 0; j <= 4; j++) {
            printf("%c", m & (1 << (4 - j)) ? a[j] : '-');
            logtmp += sprintf(logtmp, "%c", m & (1 << (4 - j)) ? a[j] : '-');
        }
        printf("\t%u\t%s\t%d\t", (*entries)[i].uid, str, (*entries)[i].size);
        printf(d ? "\033[34m\33[1m%s\033[0m\n" : "%s\n", (*entries)[i].name);
        // WARN: BUFFER OVERFLOW
        logtmp += sprintf(logtmp, "\t%u\t%s\t%d\t%s\n", (*entries)[i].uid, str,
                          (*entries)[i].size, (*entries)[i].name);
    }
    Log("%s", logbuf);
    free(buf);
    iput(ip);

    return E_SUCCESS;
}

int cmd_cat(char *name, uchar **buf, uint *len) {
    int ret = checkFmt();
    if(ret) return ret;
    uint inum = findinum(name);
    if (inum == NINODE(sb.size)) {
        Warn("cat: Not found!");
        return E_ERROR;
    }
    ret = checkPermission(inum, R);
    if(ret) return ret;
    inode *ip = iget(inum);
    checkIp(ip);
    if (ip->type != T_FILE) {
        Warn("cat: Not a file");
        iput(ip);
        return E_ERROR;
    }

    *buf = malloc(ip->size + 2);
    readi(ip, *buf, 0, ip->size);
    *buf[ip->size] = '\n';
    *buf[ip->size + 1] = '\0';

    iput(ip);
    return E_SUCCESS;
}

int cmd_w(char *name, uint len, const char *data) {
    int ret = checkFmt();
    if(ret) return ret;
    uint inum = findinum(name);
    if (inum == NINODE(sb.size)) {
        Warn("w: Not found!");
        return E_ERROR;
    }
    ret = checkPermission(inum, W);
    if(ret) return ret;
    inode *ip = iget(inum);
    checkIp(ip);
    if (ip->type != T_FILE) {
        Warn("w: Not a file");
        iput(ip);
        return E_ERROR;
    }
    if (len > 512 || len > strlen(data)) {
        Warn("w: Too long");
        iput(ip);
        return E_ERROR;
    }

    writei(ip, (uchar *)data, 0, len);

    if (len < ip->size) {
        // if the new data is shorter, truncate
        ip->size = len;
        iupdate(ip);
        itest(ip);
    }

    iput(ip);
    return E_SUCCESS;
}

int cmd_i(char *name, uint pos, uint len, const char *data) {
    int ret = checkFmt();
    if(ret) return ret;
    uint inum = findinum(name);
    if (inum == NINODE(sb.size)) {
        Warn("i: Not found!");
        return E_ERROR;
    }
    ret = checkPermission(inum, W);
    if(ret) return ret;
    inode *ip = iget(inum);
    checkIp(ip);
    if (ip->type != T_FILE) {
        Warn("i: Not a file");
        iput(ip);
        return E_ERROR;
    }
    if (len > 512 || len > strlen(data)) {
        Warn("i: Too long");
        iput(ip);
        return E_ERROR;
    }

    if (pos >= ip->size) {
        pos = ip->size;
        writei(ip, (uchar *)data, pos, len);
    } else {
        uchar *buf = malloc(ip->size - pos);
        // [pos, size) -> [pos+len, size+len)
        readi(ip, buf, pos, ip->size - pos);
        writei(ip, (uchar *)data, pos, len);
        writei(ip, buf, pos + len, ip->size - pos);
        free(buf);
    }

    iput(ip);
    return E_SUCCESS;
}

int cmd_d(char *name, uint pos, uint len) {
    int ret = checkFmt();
    if(ret) return ret;
    uint inum = findinum(name);
    if (inum == NINODE(sb.size)) {
        Warn("d: Not found!");
        return E_ERROR;
    }
    ret = checkPermission(inum, W);
    if(ret) return ret;
    inode *ip = iget(inum);
    checkIp(ip);
    if (ip->type != T_FILE) {
        Warn("d: Not a file");
        iput(ip);
        return E_ERROR;
    }

    if (pos + len >= ip->size) {
        ip->size = pos;
        iupdate(ip);
    } else {
        // [pos + len, size) -> [pos, size - len)
        uint copylen = ip->size - pos - len;
        uchar *buf = malloc(copylen);
        readi(ip, buf, pos + len, copylen);
        writei(ip, buf, pos, copylen);
        ip->size -= len;
        iupdate(ip);
        itest(ip);  // try to shrink
        free(buf);
    }

    iput(ip);
    return E_SUCCESS;
}

int cmd_login(int auid) {
    if (auid <= 0 || auid >= 1024) {
        Warn("login: Invalid uid");
        return E_ERROR;
    }
    uid = auid;
    printf("Hello, uid=%u!\n", uid);
    return E_SUCCESS;
}
