/**
  @file fs.h
  @brief 该文件是文件系统头文件，包含了与文件系统相关的结构体。
  */

#ifndef _FS_H
#define _FS_H

#include <sys/types.h>
/*
   0 - unused         没有使用到
   1 - /dev/mem       内存设备
   2 - /dev/fd        软盘设备
   3 - /dev/hd        硬盘设备
   4 - /dev/ttyx      tty串行终端设备
   5 - /dev/tty       tty终端设备
   6 - /dev/lp        打印设备
   7 - unnamed pipes  未命名的管道
  */

#define IS_SEEKABLE(x) ((x) >= 1 && (x) <= 3)

#define READ 0
#define WRITE 1
#define READA 2
#define WRRITEA 3

void buffer_init(long buffer_end);

#define MAJOR(a) (((unsigned)(a)) >> 8)        // 主设备号存放在高字节
#define MINOR(a) ((a) & 0xff)                  // 次设备号存放在低字节

#define NAME_LEN 14
#define ROOT_INO 1

#define I_MAP_SLOTS 8
#define Z_MAP_SLOTS 8
#define SUPER_MAGIC 0x137F

#define NR_OPEN 20
#define NR_INODE 32
#define NR_FILE 64
#define NR_SUPER 8
#define NR_HASH 307
#define NR_BUFFERS nr_buffers
#define BLOCK_SIZE 1024
#define BLOCK_SIZE_BITS 10

#ifndef NULL
#define NULL ((void*)0)
#endif

#define INODES_PER_BLOCK ((BLOCK_SIZE) / (sizeof (struct d_inode)))
#define DIR_PER_BLOCK ((BLOCK_SIZE) / (sizeof (struct dir_entry)))

#define PIPE_HEAD(inode) ((inode).i_zone[0])
#define PIPE_TAIL(inode) ((inode).i_zone[1])
#define PIPE_SIZE(inode) ((PIPE_HEAD(inode)-PIPE_TAIL(inode)) & (PAGE_SIZE - 1))
#define PIPE_EMPTY(inode) (PIPE_HEAD(inode) == PIPE_TAIL(inode))
#define PIPE_FULL(inode) (PIPE_SIZE(inode) == (PAGE_SIZE - 1))
#define INC_PIPE(head) __asm__("incl %0\n\t andl $4095, %0"::"m"(head))

typedef char buffer_block[BLOCK_SIZE];

struct buffer_head
{
    char* b_data;
    unsigned long b_blocknr;
    unsigned short b_dev;
    unsigned char b_uptodate;
    unsigned char b_dirt;
    unsigned char b_count;
    unsigned char b_lock;
    struct task_struct* b_wait;
    struct buffer_head* b_prev;
    struct buffer_head* b_next;
    struct buffer_head* b_prev_free;
    struct buffer_head* b_next_free;
};

struct d_inode
{
    unsigned short i_mode;
    unsigned short i_uid;
    unsigned long i_size;                // 文件的字节数
    unsigned long i_time;
    unsigned char i_gid;
    unsigned char i_nlinks;
    unsigned short i_zone[9];
};

struct m_inode
{
    unsigned short i_mode;
    unsigned short i_uid;
    unsigned long i_size;                // 文件的字节数
    unsigned long i_time;
    unsigned char i_gid;
    unsigned char i_nlinks;
    unsigned short i_zone[9];

    struct task_struct* i_wait;
    unsigned long i_atime;
    unsigned long i_ctime;
    unsigned short i_dev;
    unsigned short i_num;
    unsigned short i_count;
    unsigned char i_lock;
    unsigned char i_dirt;
    unsigned char i_pipe;
    unsigned char i_mount;
    unsigned char i_seek;
    unsigned char i_update;
};

struct file
{
    unsigned short f_mode;
    unsigned short f_flags;
    unsigned short f_count;
    struct m_inode* f_inode;
    off_t f_pos;
};

struct d_super_block
{
    unsigned short s_ninodes;
    unsigned short s_nzones;
    unsigned short s_imap_blocks;
    unsigned short s_zmap_blocks;
    unsigned short s_firstdatazone;
    unsigned short s_log_zone_size;
    unsigned long s_max_size;
    unsigned short s_magic;
};

struct super_block
{
    unsigned short s_ninodes;
    unsigned short s_nzones;
    unsigned short s_imap_blocks;
    unsigned short s_zmap_blocks;
    unsigned short s_firstdatazone;
    unsigned short s_log_zone_size;
    unsigned long s_max_size;
    unsigned short s_magic;

    struct buffer_head* s_imap[8];
    struct buffer_head* s_zmap[8];
    unsigned short s_dev;
    struct m_inode* s_isup;
    struct m_inode* s_imount;
    unsigned long s_time;
    struct task_struct* s_wait;
    unsigned char s_locck;
    unsigned char s_rd_only;
    unsigned char s_dirt;
};

struct dir_entry
{
    unsigned short inode;
    char name[NAME_LEN];
};

extern struct m_inode inode_table[NR_INODE];
extern struct file file_table[NR_FILE];
extern struct super_block super_blocks[NR_SUPER];
extern struct buffer_head* start_buffer;
extern int nr_buffers;

#endif // _FS_H
