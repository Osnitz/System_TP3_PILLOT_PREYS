//
// Created by matthieu on 16/10/24.
//

#include "main.h"

struct stat file_stat;
struct tosfs_superblock *sb;
struct  tosfs_inode *inodes;

static void getattr_3is(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi __attribute__((unused))){
    struct stat stbuf = {0};
    if (ino ==0){
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

static void lookup_3is(fuse_req_t req, fuse_ino_t parent, const char *name){
    // check if parent is a directory
    if (inodes[parent].mode != S_IFDIR) {
        fuse_reply_err(req, ENOTDIR);
    }
}

/*static void readdir_3is(fuse_req_t req, fuse_ino_t ino, size_t size,
                        off_t off, struct fuse_file_info *fi)
{

}

static void open_3is(fuse_req_t req, fuse_ino_t ino,
                     struct fuse_file_info *fi)
{

}

static void read_3is(fuse_req_t req, fuse_ino_t ino, size_t size,
                     off_t off, struct fuse_file_info *fi)
{

}*/

static struct fuse_lowlevel_ops oper = {
    .getattr	= getattr_3is,
    /*.lookup		= lookup_3is,
    .readdir	= readdir_3is,
    .open		= open_3is,
    .read		= read_3is,*/
};


int main(int argc, char * argv[])
{
    const char* filename = "Ressoures FS-20241016/test_tosfs_files";
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("tamere\n");
        perror("open");
        return 1;
    }

    if (fstat(fd, &file_stat) == -1) {
        perror("fstat");
        return 1;
    }
    size_t file_size = file_stat.st_size;

    void* file_in_memory = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (file_in_memory == MAP_FAILED) {
        perror("Error in memory mapping");
        close(fd);
        exit(EXIT_FAILURE);
    }
    //printf("Mmap allocated address: %p\n", file_in_memory);

    //map superblock
    sb = file_in_memory;
    if (sb->magic != TOSFS_MAGIC) {
        printf("Magic number is not correct\n");
        return 1;
    }
    printf("\n--- Suoer Block ---\n");
    printf("Magic number: 0x%x\n", sb->magic);
    printf("Block bitmap: "PRINTF_BINARY_PATTERN_INT32"\n", PRINTF_BYTE_TO_BINARY_INT32(sb->block_bitmap));
    printf("Inode bitmap: "PRINTF_BINARY_PATTERN_INT32"\n", PRINTF_BYTE_TO_BINARY_INT32(sb->inode_bitmap));
    printf("Block size: %u bytes\n", sb->block_size);
    printf("Total blocks: %u\n", sb->blocks);
    printf("Total inodes: %u\n", sb->inodes);
    printf("Root inode: %u\n", sb->root_inode);

    inodes = file_in_memory + TOSFS_BLOCK_SIZE * TOSFS_INODE_BLOCK;
    printf("\n--- Inodes ---\n");
    for (unsigned int i = 1; i < sb->inodes+1; i++) { // skip the first inode because it's root inode, it's empty
        printf("Inode %d: inode number = %u, block_no = %u, uid = %u, gid = %u, mode = %u, perm = %u, size = %u bytes, nlink = %u\n",
               i,
               inodes[i].inode,
               inodes[i].block_no,
               inodes[i].uid,
               inodes[i].gid,
               inodes[i].mode,
               inodes[i].perm,
               inodes[i].size,
               inodes[i].nlink);
    }


    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_chan *ch;
    char *mountpoint;
    int err = -1;

    if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
        (ch = fuse_mount(mountpoint, &args)) != NULL) {
        struct fuse_session *se;

        se = fuse_lowlevel_new_compat(NULL, &oper,
                       sizeof(oper), NULL);
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

    return err ? 1 : 0;
    return 0;
}
