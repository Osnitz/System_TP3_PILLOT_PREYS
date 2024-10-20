//
// Created by matthieu on 16/10/24.
//

#ifndef MAIN_H
#define MAIN_H
#include "Ressoures FS-20241016/tosfs.h"
#include <fcntl.h>    // Pour open()
#include <sys/stat.h> // Pour stat()
#include <sys/mman.h> // Pour mmap()
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fuse/fuse_lowlevel.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>

void getattr_3is(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void readdir_3is(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
void lookup_3is(fuse_req_t req, fuse_ino_t parent, const char *name);
void read_3is(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);


#endif //MAIN_H
