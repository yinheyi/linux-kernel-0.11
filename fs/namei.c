#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <const.h>
#include <sys/stat.h>

/**
  @brief 
  @details \004、\002、\006、\377是4个8进制的转义字符，该宏就是根据x值取4个转义字符中的一个。
  */
#define ACC_MODE(x) ("\004\002\006\377"[(x)&_ACCMODE])

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

/**
  @brief 该函数用于判断当前进程对一个inode节点相关的文件权限----rwx, 如果拥有指定的权限,则返回1,否则返回0.
  @param [in] inode  指定的inode指针
  @param [in] mask   要检测的位状态, 可以同时检测多个位，例如检测是否有读和写权限。
  @return 拥有权限返回1,没有权限返回0. 

  具体实现为：从inode的m_inode中获取文件相关的访问权限，然后与mask的值进行比较。超级用户永远具有权限，对于
  其它用户，根据当前用户是不是文件的所有者/是不是文件所有组的成员/其它用户来判断对应的权限位。
  */
static int permission(struct m_inode* inode, int mask)
{
    /* 在mode的0-2位保存了其它用户对该文件的访问权限， 3-5位保存了同一个组内用户的访问权限，6-8位保存
       了所有者的访问权限.  */
    int mode = inode->i_mode;

    // 如果一个inode的i_nlinks为0时，说明没有目录项指向它，那么也就是它已经是删除状态了
    if (inode->i_dev && !inode->i_nlinks)
        return 0;
    else if (current->euid == inode->i_uid)
        mode >>= 6;
    else if (current->egid == inode->i_gid)
        mode >>= 3;

    // if 语句中之所以判断按位与之后是否等于mask，是因为mask中可能检测的不只一个bit, 而是多个bit.所有不能
    // 仅仅判断按位与之后的结果是否非0
    if (((mode & mask & 0007 && mask) == mask) || suser())
        return 1;
    else
        return 0;
}

/**
  @brief 该函数比较name的字符串与目录项结构内存储的name的字符串是否相同。
  @param [in] len  待判断的字符串长度
  @param [in] name 待判断的字符串指针
  @param [in] de   待判断的目录项的结构的指针
  @return 当相同时返回1, 不同时返回0.

  具体实现：首先判断了待比较的字符串的长度与目录顶结构内的长度是否相同，如果不同则返回0.
  然后再通过汇编指令cmpsb来按字节比较字符串是否相同。
  */
static int match(int len, const char* name, struct dir_entry* de)
{
    register int same __asm__("ax");
    if (!de || !de->inode || len > NAME_LEN)
        return 0;

    /* 此时说明了name中的长度大于了待比较的长度，它们肯定不相同。
       当len == NAME_LEN时，只能通过下面的代码来判断是否相同了。
       */
    if (len < NAME_LEN && de->name[len])
        return 0;

    __asm__("cld\n\t"
            "fs;repe;cmpsb\n\t"
            "setz %%al"
            :"=a" (same)
            :"a"(0), "S"((long)name), "D"((long)de->name), "c"(len)
            :"cx", "di", "si");
    return same;
}

/**
  @brief 功能描述： 在指定的目录inode中，查找给定的目录名， 如果找到了，通过res_dir返回目录项结构的指针，
  通过返回值返回目录项结构所在的数据块对应的高速缓冲头指针。
  @param [in]  dir     给定的目录inode的指针的指针。
  @param [in]  name    要查找的目录名字。
  @param [in]  namelen 要查找的目录名字的长度。
  @param [out] res_dir 查找到的目录项结构的指针。
  @return 返回目录项结构所在的数据块对应的高速缓冲头指针。
  */
static struct buffer_head* find_entry(struct m_inode** dir, const char* name, int namelen, struct dir_entry** res_dir)
{
    int entries;
    int block, i;
    struct buffer_head* bh;
    struct dir_entry* de;
    struct super_block* sb;

    *res_dir = NULL;
    if (!namelen)
        return NULL;

    // 当定义了不截断路径名时，如果路径名超过了最大长度，就返回NULL，否则的话就截断路径名为最大长度。
#ifdef NO_TRUNCATE
    if (namelen > NAME_LEN)
        return NULL;
#else
    if (namelen > NAME_LEN)
        namelen = NAME_LEN;
#endif

    /* i_size为当前目录的inode中数据的大小(以字节为单位), 数据内存放了多少个目录项.
       通过下面语句就可以知道当前目录中存放了多少个目录项结构.
       */
    entries = (*dir)->i_size / (sizeof(struct dir_entry));

    /*
       下面的if语句要特别处理一下要查找 ".."目录时的情况。
       如果 dir 是当前进程的根目录，则在根目录下查找 .. 相当于查找 .
       如果 dir 是文件系统的根节点，则'..' 将导致目录交换到安装到文件系统的目录i节点。
       反正吧， 这里不太明白为什么！！！！
       */
    if (namelen == 2 && get_fs_byte(name) == '.' && get_fs_byte(name + 1) == '.')
    {
        if ((*dir) == current->root)
            namelen = 1;
        else if ((*dir)->i_num == ROOT_INO)
        {
            sb = get_super((*dir)->i_dev);
            if (sb->s_imount)
            {
                iput(*dir);
                (*dir) = sb->s_imount;
                (*dir)->i_count++;
            }
        }
    }

    // 接下来，从inode对应的数据区内查找匹配的目录项.
    if (!(block = (*dir)->i_zone[0]))
        return NULL;
    if (!(bh = bread((*dir)->idev, block)))
        return NULL;
    i = 0;
    de = (struct dir_entry*) bh->b_data;
    while (i < entries)
    {
        // 当读取完当前逻辑块时，切换到下一个逻辑块继续查找。
        if ((char*)de >= BLOCK_SIZE + b_data)
        {
            brelse(bh);
            bh = NULL;

            // 下一块为空时，继续查找下下一块。
            if (!(block = bmap(*dir, i / DIR_ENTRIES_PER_BLOCK))
                || !(bh = bread((*dir)->i_dev, block)))
            {
                i += DIR_ENTRIES_PER_BLOCK;
                continue;
            }
            de = (struct dir_entry*) bh->b_data;
        }

        // 如果查找到了，返回相应的值。
        if (match(namelen, name, de))
        {
            *res_dir = de;
            return bh;
        }
        ++i;
        ++de;
    }

    // 没有查找到时，返回NULL.
    brelse(bh);
    return NULL;
}

/**
  @brief
  @param 
  @return
  */
static struct buffer_head* add_entry(struct m_inode* dir, const char* name, int namelen, struct dir_entry** res_dir)
{
    int block, i;
    struct buffer_head* bh;
    struct dir_entry* de;
    
    *read_dir = NULL;
    if (namelen == 0)
        return NULL;
    
    // 是否进行截断处理
#ifdef NO_TRUNCATE
    if (namelen > NAME_LEN)
        return NULL;
#else
    if (namelen > NAME_LEN)
        namelen = NAME_LEN;
#endif
    
    if (!(block = dir->i_zones[0]))
        return NULL;
    if (!(bh = bread(dir->i_dev, block)))
        return NULL;
    
    i = 0;
    de = (struct dir_entry*)bh->b_data;
    while (1)
    {
        if ((char*)de >= bh->b_data + BLOCK_SIZE)
        {
        }
        if (!de->inode)
        {
            
        }
        
        ++i;
        ++de;
    }
    
    
}
