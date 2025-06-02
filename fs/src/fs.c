#include "fs.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "block.h"
#include "log.h"

ushort uid;
uint cwd;
ushort profid;

void load_user(id_map *idmap, int id){
    for(int i = 0; i < MAXUSER; ++i){
        if(idmap[i].client_id == id){
            for(int j = 0; j < MAXUSER; ++j){
                if(sb.users[j].uid == idmap[i].uid){
                    profid = j;
                    
                    uid = sb.users[profid].uid;

                    inode *ip = iget(sb.users[profid].cwd);
                    if(checkIp(ip)){
                        Error("Failed to find cwd for uid: %d, return to the root", uid);
                        cwd = 0;
                    }
                    cwd = sb.users[profid].cwd;
                    iput(ip);
                    return;
                }
            }
        }
    }
    Error("No matching userfile");
}

void save_user(id_map *idmap, int id){
    for(int i = 0; i < MAXUSER; ++i){
        if(idmap[i].client_id == id){
            if(idmap[i].uid == 0) idmap[i].uid = uid;
            sb.users[profid].uid = uid;
            sb.users[profid].cwd = cwd;
            Log("uidmap: %d: id: %d uid: %d cwd: %d", profid, id, uid, cwd);

            uchar buf[BSIZE];
            memcpy(buf, &sb, sizeof(sb));
            write_block(0, buf);

            uid = 0;
            cwd = 0;
            profid = 0;
            break;
        }
    }
}

void sbinit() {
    uchar buf[BSIZE];
    read_block(0, buf);
    memcpy(&sb, buf, sizeof(sb));
}

int to_home();

int format(int *ncyl, int *nsec){
    uchar buf[BSIZE];

    sb.magic = MAGIC;
    sb.size = (*ncyl) * (*nsec);

    int ninodes = NINODE(sb.size);
    int nmeta = 1 + NBBLOCK(sb.size) + NIBLOCK(sb.size);

    sb.ninodes = ninodes;
    sb.nblocks = sb.size - 1 - NBBLOCK(sb.size) - NIBLOCK(sb.size);
    sb.bmapstart = 1;
    sb.inodestart = 1 + NIBLOCK(sb.size);
    sb.users[0].uid = 1;
    sb.users[0].cwd = 0;

    // zero superblock
    memset(buf, 0, BSIZE);
    memcpy(buf, &sb, sizeof(sb));
    write_block(0, buf);

    // bitmap, inode block init
    memset(buf, 0, BSIZE);
    for (int i = 0; i < sb.size; i += BPB) write_block(BBLOCK(i), buf);
    for (int i = 0; i < ninodes; i += IPB) write_block(IBLOCK(i), buf);

    // mark bitmap and inode blocks as in use
    for (int i = 0; i < nmeta; i += BPB) {
        memset(buf, 0, BSIZE);
        for (int j = 0; j < BPB; ++j)
            if (i + j < nmeta) buf[j / 8] |= 1 << (j % 8);
        write_block(BBLOCK(i), buf);
    }

    cwd = 0;
    // make root dir
    if (!icreate(T_DIR, NULL, 0, 0, 0b11111)){
        Log("Disk format done");
        Log("size=%u, nblocks=%u, ninodes=%u", sb.size, sb.nblocks, sb.ninodes);
    }
    else return -1;
    
    // to_home();
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
        Warn("Not formatted");
        return E_NOT_FORMATTED;
    }
    return ret ? E_NOT_LOGGED_IN : E_SUCCESS;
}

int checkVisible(uint inum){
    inode *ip = iget(inum);
    checkIp(ip);
    int ret = 1;
    if(uid == 1 || ip->uid == uid){
        ret = 1;
    } else if((ip->mode & 0b1) == 0b1){
        ret = 1;
    } else {
        ret = 0;
    }
    iput(ip);
    return ret;
}

int checkPermission(uint inum, short perm){
    inode *ip = iget(inum);
    checkIp(ip);
    int ret = 0;
    if (uid == 1)
        ret = 1;
    else if(ip->uid == uid) {
        if ((ip->mode >> 3 & perm) == perm)
            ret = 1;
        else{
            ret = 0;
            Warn("Permisson denied");
        }
    } else {
        if ((ip->mode >> 1 & perm) == perm)
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
    // for (int i = 0; i < len; ++i)
    //     for (int j = 0; j < invalidlen; ++j)
    //         if (name[i] == invalid[j]) return 0;
    return 1;
}

// find inum from cwd
uint findinum(char *name) {
    inode *ip = iget(cwd);
    checkIp(ip);
    
    uchar *buf = malloc(ip->size);
    readi(ip, buf, 0, ip->size);
    dirent *dir = (dirent *)buf;

    int ninodes = NINODE(sb.size);
    int result = ninodes;
    int nfile = ip->size / sizeof(dirent);
    for (int i = 0; i < nfile; ++i) {
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

// delete inode from cwd
int delinum(uint inum) {
    inode *ip = iget(cwd);
    checkIp(ip);

    uchar *buf = malloc(ip->size);
    readi(ip, buf, 0, ip->size);
    dirent *dir = (dirent *)buf;

    int ninodes = NINODE(sb.size);
    int nfile = ip->size / sizeof(dirent);
    int deleted = 1;
    for (int i = 0; i < nfile; ++i) {
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
        for (int i = 0; i < nfile; ++i) {
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
    if(uid != 1) return E_PERMISSION_DENIED;
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
    ret = checkPermission(cwd, R | W);
    if(ret) return ret;
    if (!is_name_valid(name)) {
        Warn("mk: Invalid name!");
        return E_ERROR;
    }
    if (findinum(name) != NINODE(sb.size)) {
        Warn("mk: %s already exists!", name);
        return E_ERROR;
    }
    if(icreate(T_FILE, name, cwd, uid, mode)) return E_ERROR;
    return E_SUCCESS;
}

int cmd_mkdir(char *name, short mode) {
    int ret = checkFmt();
    if(ret) return ret;
    ret = checkPermission(cwd, R | W);
    if(ret) return ret;
    if (!is_name_valid(name)) {
        Warn("mkdir: Invalid name!");
        return E_ERROR;
    }
    if (findinum(name) != NINODE(sb.size)) {
        Warn("mkdir: %s already exists!", name);
        return E_ERROR;
    }
    if(icreate(T_DIR, name, cwd, uid, mode)) return E_ERROR;
    return E_SUCCESS;
}

int cmd_rm(char *name) {
    int ret = checkFmt();
    if(ret) return ret;
    uint inum = findinum(name);
    if (inum == NINODE(sb.size)) {
        Warn("rm: Not found!");
        return E_ERROR;
    }
    inode *ip = iget(inum);
    checkIp(ip);
    if (ip->type != T_FILE) {
        Warn("rm: Not a file, please use rmdir");
        iput(ip);
        return E_ERROR;
    }
    ret = checkPermission(inum, W);
    ret |= checkPermission(cwd, R | W);
    if(ret){
        iput(ip);
        return ret;
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
    inode *ip = iget(inum);
    checkIp(ip);
    if (ip->type != T_DIR) {
        Warn("rmdir: Not a directory, please use rm");
        iput(ip);
        return E_ERROR;
    }
    ret = checkPermission(inum, R | W);
    ret |= checkPermission(cwd, R | W);
    if(ret){
        iput(ip);
        return ret;
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

int _cd(char *name) {
    uint inum = findinum(name);
    if (inum == NINODE(sb.size)) {
        Warn("cd: Not found!");
        return E_ERROR;
    }
    int ret = checkPermission(inum, R);
    if(ret) return ret;
    inode *ip = iget(inum);
    checkIp(ip);
    if (ip->type != T_DIR) {
        Warn("cd: Not a directory");
        iput(ip);
        return E_ERROR;
    }
    cwd = inum;
    iput(ip);
    return 0;
}

int cmd_cd(char *str) {
    int ret = checkFmt();
    if(ret) return ret;
    char *ptr = NULL;
    int backup = cwd;
    if (str[0] == '/') cwd = 0;  // start from root
    char *p = strtok_r(str, "/", &ptr);
    while (p) {
        ret = _cd(p);
        if (ret != 0) {  // if not success
            cwd = backup;   // restore the cwd
            return ret;
        }
        p = strtok_r(NULL, "/", &ptr);
    }
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
    if(ret) return ret;
    ret = checkPermission(cwd, R);
    if(ret) return ret;
    inode *ip = iget(cwd);
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
        if(checkVisible(dir[i].inum) == 0) continue;  // invisible
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
    inode *ip = iget(inum);
    checkIp(ip);
    if (ip->type != T_FILE) {
        Warn("cat: Not a file");
        iput(ip);
        return E_ERROR;
    }
    ret = checkPermission(inum, R);
    if(ret){
        iput(ip);
        return ret;
    }

    if(*buf != NULL){
        Warn("cat: Dirty buffer");
        // return E_ERROR;
    } 
    *buf = malloc(ip->size + 2);
    readi(ip, *buf, 0, ip->size);
    (*buf)[ip->size] = '\n';
    (*buf)[ip->size + 1] = '\0';
    *len = strlen((const char *)*buf);

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
    if (len > BSIZE|| len > strlen(data)) {
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
    if (len > BSIZE || len > strlen(data)) {
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

int to_home(){
    int ret = checkFmt();
    if(ret) return ret;
    uint backup = cwd;
    ret = cmd_cd("/home");
    if(ret != E_SUCCESS){
        Warn("to_home: Home directory not found");
        ret = cmd_mkdir("home", 0b11111);
        if(ret != E_SUCCESS){
            Error("to_home: Failed to create home directory");
            return ret;
        }
        ret = cmd_cd("/home");
        if(ret != E_SUCCESS){
            Error("to_home: Failed to cd /home");
            return ret;
        }
    }

    char uid_home[MAXNAME];
    sprintf(uid_home, "%d", uid);
    ret = cmd_cd(uid_home);
    if(ret != E_SUCCESS){
        ret = cmd_mkdir(uid_home, 0b111111);
        if(ret != E_SUCCESS){
            Error("to_home: Failed to creart user home dir");
            return ret;
        }
        ret = cmd_cd(uid_home);
        if(ret != E_SUCCESS){
            Error("to_home: Failed to cd user home");
            return ret;
        }
    }

    cwd = backup;
    return E_SUCCESS;
}

int cmd_login(int auid) {
    if (auid <= 0 || auid >= 1024) {
        Warn("login: Invalid uid");
        return E_ERROR;
    }
    uid = auid;
    // to_home();
    return E_SUCCESS;
}

int cmd_exit() {
    uid = 0;
    cwd = 0;
    return E_SUCCESS;
}

int cmd_pwd(char **msg, uint *len){
    int ret = checkLogin();
    if(ret) return ret;
    ret = checkPermission(uid, R);
    if(ret) return ret;
    inode *ip = iget(cwd);
    checkIp(ip);

    char name[256][MAXNAME];
    uchar *buf = malloc(ip->size);
    readi(ip, buf, 0, ip->size);
    dirent *dir = (dirent *)buf;
    int nfile = ip->size / sizeof(dirent), ndir = 0, curr = cwd;
    *len = 0;
    while(ip->inum != 0){
        if(nfile < 2){
            Error("pwd: No parent");
            return E_ERROR;
        }
        iput(ip);
        ip = iget(dir[1].inum);
        checkIp(ip);

        free(buf);
        buf = malloc(ip->size);
        readi(ip, buf, 0, ip->size);
        dir = (dirent *)buf;
        nfile = ip->size / sizeof(dirent);
        for(int i = 0; i < nfile; ++i){
            if(dir[i].inum == curr){
                strcpy(name[ndir++], dir[i].name);
                *len += strlen(dir[i].name) + 1;
                // Log("%s %d", dir[i].name, *len);
                curr = ip->inum;
                break;
            }
        }
        if(curr != ip->inum){
            Error("pwd: Current dir name not found");
            return E_ERROR;
        }
    }
    if(*msg != NULL){
        Warn("pwd: Dirty pwd");
    }
    
    *msg = malloc(max(2, *len + 1));
    *msg[0] = 0;
    if(ndir == 0){
        ++(*len);
        strcat(*msg, "/");
    }
    for(int i = 0; i < ndir; ++i){
        strcat(*msg, "/");
        strcat(*msg, name[ndir - i - 1]);
        // Log("msg: %s", *msg);
    }
    
    free(buf);
    iput(ip);
    return E_SUCCESS;
}