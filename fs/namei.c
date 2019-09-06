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
  @brief 该函数实现向给定的一个目录中添加一个新的目录项名。
  @param [in] dir 给定的一个目录对应的inode指针。
  @param [in] name 要添加的目录项名字
  @param [in] namelen 要添加的目录项名字的长度
  @param [out] res_dir 通过该值返回指向新添加的目录项的指针。
  @return 返回新添加目录项所在的
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
        // 如果当前逻辑块已经查找完时，再继续查找下一个逻辑块(可能是新创建的逻辑块，也可能之前的吧。
        if ((char*)de >= bh->b_data + BLOCK_SIZE)
        {
            if (!(block = create_block(dir, i / DIR_ENTRIES_PER_BLOCK)))
                return NULL;
            
            // 当前逻辑块的指针为空时，就继续查找下一个逻辑块，那它什么时候会为空呢？
            if (!(bh = bread(dir->i_dev, block))) 
            {
                i += DIR_ENTRIES_PER_BLOCK;
                continue;
            }
            de = (struct dir_entry*)bh->b_data;
        }
        
        // 当查找到了新的数据区时（已经大于的原来的size大小）时，肯定可以在这里加入entry的：因此改变一下
        // inode的对应的数据文件大小，接下来一定要改变一下文件的改变时间了(i_ctime).
        // i_ctime 表示文件的改变时间， 只要文件改变了就更新(不局限于内容改变）,例如访问权限啦，文件大小啊，等。
        // 而i_mtime 表示修改时间，仅局限于文件内容改变时才更新该值。
        if (i * sizeof(struct dir_entry) >= dir->i_size)
        {
            dir->size = (i+1) * sizeof(struct dir_entry);
            dir->i_ctime = CURRENT_TIME;
            dir->i_dirt = 1;
            
            de->inode = 0;        // 修改为0原因是： 接下来的代码会告诉你答案！
        }
        
        // 当struct entry中的inode值为0时，表示该entry可以使用！
        // 有一点不明白，下面的if语句内使用了该entry之后，为什么不把de-inode置为非0呢？
        // 万一又被其它进程使用了怎么办？
        if (!de->inode)
        {
            for (i = 0; i < NAME_LEN; i++)
                de->name[i] = (i < namelen) ? get_fs_byte(name+i) : 0;
            dir->i_mtime = CURRENT_TIME;
            bh->b_dirt = 1;
            *res_dir = de;
            return bh;
        } 
        ++i;
        ++de;
    }
    brelse(bh);
    return NULL;
}

/**
  @brief 该函数一层层的遍历给定的一个路径，形如："/gd/ewr/gd/sdsf"的形式， 返回最后一个目录对应的inode指针。
  @param [in] pathname 路径名.
  @return 返回最后一个目录对应的inode指针。  
  
  1. 路径名如果是以/开头，表示绝对路径，形如：/ab/dd/adf/
  2. 路径名如果是字母开头，表示相对路径，形如：ab/dde/sda/  
  3. 形如这样的路径：/abc/bcd/ert/xiao, 会返回ert目录对应的inode.
  4. 形如这样的路径：/sadf/xsad/xiao/ 会返回xiao目录对应的inode.
  5. 输入空路径时：''，返回当前的目录。
  6. 不支持形如这样的路径：/adfds/asdf//dfsa//asdf/
  */ 
static struct m_inode* get_dir(const char* pathname)
{
    char c;
    const char* thisname;
    struct m_inode* inode;
    strcut buffer_head* bh;
    int namelen, inr, idev;
    struct dir_entry* de;
    
    // 在进行处理之前先判断一些必须满足的条件：
    if (!pathname)
        return NULL;
    
    if (!current->root || !current->root->i_count)
        panic("No root inode");
    if (!current->pwd || !current->pwd->i_count)
        panic("No cwd inode");
    
    // 通过路径名中的第一个字符判断是绝对路径/相对路径/空路径
    if ((c = get_fs_byte(pathname)) == '/')
    {
        inode = current->root;
        ++pathname;
    }
    else if (c)
        inode = current->pwd;
    else
        return NULL;
    
    // 接下来进行一层层地查找过程：
    while (1)
    {
        if (!S_ISDIR(inode->i_mode) || !permission(inode, MAY_EXEC))
        {
            iput(inode);
            return NULL;
        }
        
        // 确定下一级目录的名字(可能为0，也可能不为0)
        thisname = pathname;
        for (namelen = 0; (c = get_fs_byte(pathname++)) && (c != '/'); namelen++)
            /* do noting */;
        
        // 此时对应了这样的目录： ad/dsf/dsf/sdf 或 er/wg/sdf/
        if (!c)
            return inode;
        
        // 从下面的代码中可以看出来，不支持形如'//'的目录，因为对应的namelen为0， 会返回NULL.
        if (!(bh= find_entry(&inode, thisname, namelen, &de)))
        {
            iput(inode);
            return NULL;
        }
        inr = de->inode;        // 里面存放了该目录项的指向的inode在磁盘中的索引号。
        idev = inode->i_dev;
        brelse(bh);
        iput(inode);
        if (!(inode = iget(idev, inr)))
            return NULL;
    }
}
