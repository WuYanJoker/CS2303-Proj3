#include "disk.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

#include "log.h"

// global variables
int _ncyl, _nsec, _ttd;
char *diskfile;

int init_disk(char *filename, int ncyl, int nsec, int ttd) {
    // do some initialization...
    if(ncyl < 0 || nsec < 0 || ttd < 0){
        Log("Disk init: Invalid parameter");
        return -1;
    }
    _ncyl = ncyl;
    _nsec = nsec;
    _ttd = ttd;
    // open file
    int fd = open(filename, O_RDWR | O_CREAT, 0666);
    // printf("%d\n", fd);
    if(fd < 0){
        Log("Error: Could not open file '%s' .\n", filename);
        return -1;
    }
    // stretch the file
    long FILESIZE = BLOCKSIZE * _nsec * _ncyl;
    int result = lseek(fd, FILESIZE-1, SEEK_SET);
    if(result == -1){
        Log("Error calling lseek() to 'stretch' the file");
        close(fd);
        return -1;
    }
    result = write(fd, "", 1);
    if(result != 1){
        Log("Error writing last byte of the file");
        close(fd);
        return -1;
    }
    // mmap
    diskfile = (char *) mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    // printf("Initialled %p\n", diskfile);
    if(diskfile == MAP_FAILED){
        close(fd);
        Log("Error: Could not map file.\n");
        return -1;
    }
    close(fd);
    Log("Disk initialized: %s, %d Cylinders, %d Sectors per cylinder", filename, ncyl, nsec);
    return 0;
}

void delay(int cyl){
    static int curr_cyl;
    usleep(1000 * _ttd * abs(curr_cyl - cyl));
    curr_cyl = cyl;
}

// all cmd functions return 0 on success
int cmd_i(int *ncyl, int *nsec) {
    // get the disk info
    *ncyl = _ncyl;
    *nsec = _nsec;
    Log("%d Cylinders, %d Sectors per cylinder", _ncyl, _nsec);
    return 0;
}

int cmd_r(int cyl, int sec, char *buf) {
    // read data from disk, store it in buf
    if (cyl >= _ncyl || sec >= _nsec || cyl < 0 || sec < 0) {
        Log("Invalid cylinder or sector");
        return 1;
    }
    delay(cyl);
    int n = cyl * _nsec + sec;
    memcpy(buf, diskfile + BLOCKSIZE * n, BLOCKSIZE);
    return 0;
}

int cmd_w(int cyl, int sec, int len, char *data) {
    static long sys_pagesize = 0;
    // write data to disk
    if (cyl >= _ncyl || sec >= _nsec || cyl < 0 || sec < 0) {
        Log("Invalid cylinder or sector");
        return 1;
    }
    if (len > 512) {
        Log("Too long data");
        return 1;
    }
    delay(cyl);
    int n = cyl * _nsec + sec;
    // printf("%d %p\n", n, diskfile);
    char *disk_addr = diskfile + BLOCKSIZE * n;
    memcpy(disk_addr, data, len);

    if(sys_pagesize == 0) sys_pagesize = sysconf(_SC_PAGESIZE);
    if(sys_pagesize == -1) sys_pagesize = 4096;

    char *mapped_end = diskfile + (BLOCKSIZE * _ncyl * _nsec);
    char *sync_start = (char *)((uintptr_t)disk_addr & ~(sys_pagesize - 1));
    size_t sync_length = (sync_start + 2 * sys_pagesize) > mapped_end ? (mapped_end - sync_start) : 2 * sys_pagesize;
    if(msync(sync_start, sync_length, MS_SYNC) == -1) return 1;
    return 0;
}

void close_disk() {
    // close the file
    if (diskfile != NULL && diskfile != MAP_FAILED) {
        long FILESIZE = BLOCKSIZE * _ncyl * _nsec;
        msync(diskfile, FILESIZE, MS_SYNC);
        if(munmap(diskfile, FILESIZE) == -1){
            Log("Error: Munmap failed.");
        }
        diskfile = NULL;
    }
}
