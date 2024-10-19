//
// Created by matthieu on 16/10/24.
//

#include "main.h"
#define FUSE_VERSION 26

struct stat file_stat;
struct tosfs_superblock *sb;
struct  tosfs_inode *inodes;
void *file_in_memory;


static void getattr_3is(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi __attribute__((unused))){
    struct stat stbuf = {0};
    if (ino == 0 || ino > sb->inodes) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    stbuf.st_ino = ino;
    stbuf.st_uid = inodes[ino].uid;
    stbuf.st_gid = inodes[ino].gid;
    stbuf.st_mode = inodes[ino].mode;
    stbuf.st_nlink = inodes[ino].nlink;
    stbuf.st_size = inodes[ino].size;
    stbuf.st_blksize = sb->block_size;
    stbuf.st_blocks = inodes[ino].size / sb->block_size;
    fuse_reply_attr(req, &stbuf, 1.0);
}

static void lookup_3is(fuse_req_t req, fuse_ino_t parent, const char *name) {
    struct fuse_entry_param e;

    // Check if the parent inode is a directory
    if (inodes[parent].mode != S_IFDIR) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }

    // Obtain the data block associated with this inode
    struct tosfs_dentry *dentries = (struct tosfs_dentry *) ((char*)file_in_memory + inodes[parent].block_no * TOSFS_BLOCK_SIZE);

    // Cross the dentries and find the file
    for (int i = 0; i < TOSFS_BLOCK_SIZE / sizeof(struct tosfs_dentry); i++) {
        if (strncmp(dentries[i].name, name, TOSFS_MAX_NAME_LENGTH) == 0) {
            // File found, send the entry back
            memset(&e, 0, sizeof(e));
            e.ino = dentries[i].inode;
            e.attr.st_ino = e.ino;
            e.attr.st_uid = inodes[e.ino].uid;
            e.attr.st_gid = inodes[e.ino].gid;
            e.attr.st_mode = inodes[e.ino].mode;
            e.attr.st_nlink = inodes[e.ino].nlink;
            e.attr.st_size = inodes[e.ino].size;
            e.attr.st_blksize = sb->block_size;
            e.attr.st_blocks = inodes[e.ino].size / sb->block_size;

            fuse_reply_entry(req, &e);
            return;
        }
    }

    fuse_reply_err(req, ENOENT);
}


static void readdir_3is(fuse_req_t req, fuse_ino_t ino, size_t size,
                        off_t off, struct fuse_file_info *fi){
    if (inodes[ino].mode != S_IFDIR) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }

    struct tosfs_dentry *dentries = file_in_memory + inodes[ino].block_no * TOSFS_BLOCK_SIZE;

    char buffer[size];
    size_t total_size = 0;

    for (int i = 0; i < TOSFS_BLOCK_SIZE / sizeof(struct tosfs_dentry); i++) {
        struct tosfs_dentry *dentry = &dentries[i];

        if (dentry->inode == 0) {
            continue;
        }

        struct stat stbuf = {0};
        stbuf.st_ino = dentry->inode;

        // Add the directory entry to the buffer
        size_t entry_size = fuse_add_direntry(req, buffer + total_size, size - total_size, dentry->name, &stbuf, total_size + 1);
        if (entry_size > size - total_size) {
            break;
        }
        total_size += entry_size;
    }

    fuse_reply_buf(req, buffer, total_size);
}



static void read_3is(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi) {
    if (ino == 0 || ino > sb->inodes) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    if ((inodes[ino].mode & S_IFMT) == S_IFDIR) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    if (!(inodes[ino].perm & S_IRUSR)) {
        fuse_reply_err(req, EACCES);
        return;
    }

    // Compute the size to read
    if (off >= inodes[ino].size) {
        fuse_reply_buf(req, NULL, 0);
        return;
    }

    if (off + size > inodes[ino].size) {
        size = inodes[ino].size - off;
    }

    void *file_data_block = (void *)((char *)file_in_memory + TOSFS_BLOCK_SIZE * inodes[ino].block_no);

    fuse_reply_buf(req, (char *)file_data_block + off, size);
}


static struct fuse_lowlevel_ops oper = {
    .getattr	= getattr_3is,
    .readdir	= readdir_3is,
    .lookup		= lookup_3is,
    .read		= read_3is,
};


int main(int argc, char *argv[]) {
    const char *filename = "Ressoures FS-20241016/test_tosfs_files";
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    if (fstat(fd, &file_stat) == -1) {
        perror("fstat");
        return 1;
    }
    size_t file_size = file_stat.st_size;

    file_in_memory = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (file_in_memory == MAP_FAILED) {
        perror("Error in memory mapping");
        close(fd);
        exit(EXIT_FAILURE);
    }

    sb = file_in_memory;
    if (sb->magic != TOSFS_MAGIC) {
        printf("Magic number is not correct\n");
        return 1;
    }

    inodes = file_in_memory + TOSFS_BLOCK_SIZE * TOSFS_INODE_BLOCK;

    /*
    printf("\n--- Super Block ---\n");
    printf("Magic number: 0x%x\n", sb->magic);
    printf("Block bitmap: "PRINTF_BINARY_PATTERN_INT32"\n", PRINTF_BYTE_TO_BINARY_INT32(sb->block_bitmap));
    printf("Inode bitmap: "PRINTF_BINARY_PATTERN_INT32"\n", PRINTF_BYTE_TO_BINARY_INT32(sb->inode_bitmap));
    printf("Block size: %u bytes\n", sb->block_size);
    printf("Total blocks: %u\n", sb->blocks);
    printf("Total inodes: %u\n", sb->inodes);
    printf("Root inode: %u\n", sb->root_inode);*/

    /*printf("\n--- Inodes ---\n");
    for (unsigned int i = 1; i < sb->inodes + 1; i++) {
        printf("Inode %d: inode number = %u, block_no = %u, uid = %u, gid = %u, mode = %u, perm = %u, size = %u bytes, nlink = %u\n",
               i, inodes[i].inode, inodes[i].block_no, inodes[i].uid,
               inodes[i].gid, inodes[i].mode, inodes[i].perm,
               inodes[i].size, inodes[i].nlink);*/

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_chan *ch;
    char *mountpoint = NULL;
    int err = -1;

    if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
        (ch = fuse_mount_compat22(mountpoint, &args)) != NULL) {
        struct fuse_session* se = fuse_lowlevel_new(&args, &oper, sizeof(oper), NULL);
        if (se != NULL) {
            if (fuse_set_signal_handlers(se) != -1) {
                fuse_session_add_chan(se, ch);
                err = fuse_session_loop(se);
                fuse_remove_signal_handlers(se);
                fuse_session_remove_chan(ch);
            }
            fuse_session_destroy(se);
        }
        fuse_unmount(mountpoint);
    }

    fuse_opt_free_args(&args);

    munmap(file_in_memory, file_stat.st_size);
    close(fd);
    return err ? 1 : 0;
}

