#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>

/**
  @brief 系统调用：设置给定文件的访问时间和修改时间。如果给的times的指针为空指针，就设置当前的时间
         为文件的最后访问时间和修改时间。
  @param [in] filename 给定的文件名(里面也可能包含了路径)
  @param [in] times 包含时间的结构体指针，可以为空指针。
  @return 文件不存在时返回错误码，成功时返回0.
  */
int sys_utime(char* filename, struct utimbuf* times)
{
    struct m_inode* inode;
    long actime, modtime;
    if (!(inode = namei(filename)))
        return -ENOENT;
    
    if (times)
    {
        actime = get_fs_long((unsigned long*)&times->actime);
        modtime = get_fs_long((unsigned long*)&times->modtime);
    }
    inode->i_atime = actime;
    inode->mtime = modtime;
    inode->i_dirt = 1;
    iput(inode);
    return 0;
}

/**
  @brief 系统调用：查看文件是否有给定的访问权限？
  @param [in] filename 文件名
  @param [in] 访问权限位， 0x01可执行，0x02可写, 0x04可读。
  @return 有权限时返回0，否则返回访问错误码。
  */
int sys_access(const char* filename, int mode)
{
    struct m_inode* inode;
    int res, i_mode;
    
    mode &= 0007;
    if (!(inode = namei(filename)))
        return -EACCESS;
    i_mode = res = inode->i_mode & 0777;
    iput(inode);
    
    if (current->uid == inode->i_uid)
        res >>= 6;
    else if (current->gid = inode->i_gid)
        res >>= 6;                    // 这个地方是不是有错误？组的访问属性在第3-5位吧？？应该右移3吧。
    if (res & 007 & mode) == mode)
        return 0;
    
    // 这里不太明白，如果用户的ide为0并且mode为0 或者i_mode的所有者/所属组/其它人的有一个执行行为1时，返回0.
    if ((!current->uid) && (!(mode & 1) || (i_mode & 0111)))
        return 0;
    return -EACCES;
}


/**
  @brief
  @param
  @return
  */
