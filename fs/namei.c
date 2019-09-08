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

  有一点不明白：O_ACCMODE为值为0x03, 而文件访问模式为：只读(0x00), 只写(0x01)和读写(0x02)，并且
  只有使用三者中的一个，所以呢， (x & O_ACCMODE)的只可能为0, 1或2, 不可能取到3, 因此呢，ACC_MODE
  宏不可能返回377的值了。
  */
#define ACC_MODE(x) ("\004\002\006\377"[(x) & O_ACCMODE])

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

    // if 语句中之所以判断按位与之后是否等于mask，是因为mask中可能检测的不只一个bit, 而是多个bit.所以不能
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

/**
  @brief 该函数实现：获取给定路径中最后一个目录的inode，并且还获取最后一个目录内包含的文件名字。
  @param [in]  pathname 给定的路径
  @param [out] namelen  返回的目录内包含的文件名的长度
  @param [out] name     返回的目录内包含的文件名
  @return 返回值为最顶层目录对应的inode节点。
  */
static struct m_inode* dir_namei(const char* pathname, int* namelen, const char** name)
{
    char c;
    const char* basename;
    struct m_inode* dir;
    
    if (!(dir = get_dir(pathname)))
        return NULL;
    
    basename = pathname;
    // 我怎么感觉这个while有bug呢？如果路径是这样的： asfd/sdfdsew/sdfds/sdf/
    // 那么得到的basename为空啊，但是dir对应的是目录sdf。
    // 也有一种可能：那就是返回的name是包含在目录内的name,而不是目录的name.
    while (c = get_fs_byte(pathname++))
    {
        if (c == '/')
            basename = pathname;
    }
    *namelen = pathname - basename - 1;
    *name = basename;
    return dir;
}

/**
  @brief 该函数获取给定路径的inode节点。例如：对于路径 /etc/lib/aa/, 会返回目录aa的inode;
  对于路径/etc/lib/xiao, 会返回文件或目录xiao的inode.  
  @param [in] pathname 给定的路径名
  @return 返回路径最后那个目录或文件的inode.
  */
struct m_inode* namei(const char* pathname)
{
    const char* basename;
    int inr, dev, namelen;
    struct m_inode* dir;
    struct buffer_head* bh;
    struct dir_entry* de;
    
    // 空的路径
    if (!(dir = dir_namei(pathname, &namelen, &basename)))
        return NULL;
    
    // 此时对应了这样的路径： /etc/adb/cede/,  它就返回目录cede的inode.
    if (!namelen) 
        return dir;
    
    if (!(bh = find_entry(&dir, basename, namelen, &de)))
    {
        iput(dir);
        return NULL;
    }
    
    // 想找最后一个目录内的文件名的inode. 形如这样的路径： /etc/lib/a.so, 即查找
    // lib目录下的a.so文件的inode.
    inr = de->inode;
    dev = dir->i_dev;
    brelse(bh);
    iput(dir);
    dir = iget(dev, inr);
    if (dir)
    {
        dir->i_atime = CURRENT_TIME;
        dir->i_dirt = 1;
    }
    return dir;
}

/**
  @brief 该函数以指定的模式打开给定的路径，并返回相应的inode节点。
  @param [in]  pathname  给定的路径名
  @param [in]  flag      打开文件的标志，例如：只读方式/只写方式/不存在时创建等。
  @param [in]  mode      文件的访问属性， 它的作用是当打开的文件不存在时，设置新创建文件的访问属性。
  @param [out] re_inode  返回相应的inode.
  @return 成功时返回0，失败时返回相应的错误码。

  flag主要控制了打开的行为：目录和文件的行为是不一样的，文件存在或不存在的行为也是不一样的。
  目录不能是只写/读写/截断/新建，当是文件时，如果文件不存在要不要创建呢？当以读方式打开文件时，如果文件
  没有读权限呢？当以写方式打开文件时，如果文件没有写权限呢？等。
  */
int open_namei(const char* pathname, int flag, int mode, struct m_inode** res_inode)
{
    const char* basename;
    int inr, dev, namelen;
    struct m_inode *dir, *inode;
    struct buffer_head* bh;
    struct dir_entry* de;

    /* 如果以截断标记打开一个只读模式的文件时，把打开文件模式修改为只写模式。
       O_ACCMODE是0x03,即取低2位, O_RDONLY是0x00, O_WRONLY是0x01. */
    if ((flag & O_TRUNC) && !(flag & O_ACCMODE))
        flag |= O_WRONLY;

    /* 使用进程的文件访问许可屏蔽码，屏蔽掉给定模式中的相应位，并添加上谱通文件的标记。
       在umask中，被屏蔽的相应位置1了。*/
    mode &= 0777 & ~current->umask; 
    mode |= I_REGULAR;

    /* 打开路径最顶层目录的inode. 例如: /etc/ad/etc/ad 会打开etc的目录inode，而/etc/ad/acd/
       会打开目录acd的目录inode.  */
    if (!(dir = dir_namei(pathname, &namelen, &basename)))
        return -ENOENT;

    // 对应的形如这样的路径： /etc/adc/dae/
    if (!namelen)
    {
        /* 如果打开操作是[创建的方式]或者是[截断0方式]或者[可写的]的操作，则返回错误码。 意思
           就是说：目录不能以创建的方式/不能以截断0的方式/不能以写的方式打开.  我不明白的是：
           为什么不能以写的方式打开呢？ */
        if (flag & (O_ACCMODE | O_CREATE | OTRUNC))
        {
            iput(dir);
            return -EISDIR;
        }
        *res_inode = dir;
        return 0;
    }

    /* 找到目录内对应名字的目录项(de)，和目录项所在的高速缓冲块的头指针。 假如没有找到的话，
       就需要新建了，具体能不能新建，还有一些约束条件。 */
    bh = find_entry(&dir, basename, namelen, &de);
    if (!bh)
    {
        if (!(flag & O_CREAT))
        {
            iput(dir);
            return -ENOENT;
        }
        if (!permission(dir, MAY_WRITE))
        {
            iput(dir);
            return -EACCES;
        }

        // 新建inode
        inode = new_inode(dir->i_dev);
        if (!inode)
        {
            iput(dir);
            return -ENOSPC;
        }
        inode->i_uid = current->euid;
        inode->i_mode = mode;
        inode->i_dirt = 1;

        // 新建entry.
        bh = add_etry(dir, basename, namelen, &de);
        if (!bh)
        {
            inode->i_nlinks--;
            inode(inode);        // 该操作不会自动减 n_links吗？
            iput(dir);
            return -ENOSPC;
        }
        de->inode = inode->i_num;
        bh->b_dirt = 1;
        brelse(bh);
        iput(dir);
        *res_inode = inode;
        return 0;
    }

    // 如果对应的目录项存在时，打开目录项中对应的inode.
    inr = de->inode;
    dev = dir->i_dev;
    brelse(bh);
    iput(dir);
    if (flag & O_EXCL)
        return -EEXIST;
    if (!(inode = iget(dev, inr)))
        return -EACCES;

    // 对相应的inode进行判断与操作。
    if (S_ISDIR(inode->i_mode) && (flag & O_ACCMODE)      // 目录不能写，为什么？
            || !permission(inode, ACC_MODE(flag)))
    {
        iput(inode);
        return -EPERM;
    }
    inode->i_atime = CURRENT_TIME;
    if (flag & O_TRUNC)
        truncate(inode);
    *res_inode = inode;
    return 0;
}

/**
  @brief 系统调用：创建一个普通的文件或特殊文件, 只有超级用户可以执行该函数。
  @param [in] filename 文件路径名
  @param [in] mode     创建的inode的模式。
  @param [in] dev      如果创建的是块设备文件或字符设备文件，则dev为相应的设备号。
  @return 成功时返回0,失败时返回相应的错误码。
  */
int sys_mknod(const char* filename, int mode, int dev)
{
    const char* basename;
    int namelen;
    struct m_inode *dir, *inode;
    struct buffer_head* bh;
    struct dir_entry* de;

    if (!super())        // 为什么只有超级用户才有权限呢？
        return -EPERM;

    if (!(dir = dir_namei(filename, &namelen, &basename)))
        return -ENOENT;
    if (!namelen)      // 对应了这样的路径： /etc/ad/edf/
    {
        iput(dir);
        return -ENOENT;
    }
    if (!permission(dir, MAY_WRITE))      // 目录没有写权限
    {
        iput(dir);
        return -EPERM;
    }

    // 当文件存在时，返回错误码，如果不存在，则创建相应的inode
    bh = find_entry(&dir, basename, namelen, &de);
    if (!bh)
    {
        brelse(bh);
        iput(dir);
        return -EEXIST;
    }
    inode = new_inode(dir->i_dev);
    if (!inode)
    {
        iput(dir);
        return -ENOSPC;
    }

    /* 设置inode的相关属性. 如果创建的是块设备文件或字符设备文件，则 inode中的第
       一个直接块(i_zone[0])中存在设备号。  */
    if (S_ISBLK(mode) || S_ISCHR(mode))
        inode->i_zone[0] = dev;
    inode->i_mode = mode;
    inode->i_mtime = CURRENT_TIME;
    inode->i_atime = CURRENT_TIME;
    inode->i_dirt = 1;

    // 创建文件相应的目录项, 并把inode的i_num放到目录项中。
    bh = add_entry(dir, basename, namelen, &de);
    if (!bh)
    {
        iput(dir);
        inode->i_nlinks = 0;
        iput(inode);
        return -ENOSPC;
    }
    de->inode = inode->i_num;
    bh->b_dirt = 1;
    iput(dir);
    iput(inode);
    brelse(bh);
    return 0;
}

/**
  @brief 系统调用：该函数实现创建一下目录，并在新创建的目录中写入两个默认的目录项. 和 ..
  @param [in] 目录的路径名
  @param [in] 新创建的目录的mode
  @return 成功时返回0, 失败时返回错误码。
  */
int sys_mkdir(const char* pathname, int mode)
{
    const char* basename;
    int namelen;
    struct m_inode *dir, *inode;
    struct buffer_head *bh, *dir_block;
    struct dir_entry *de;

    // 为什么只有超级用户才具有这个权限呢？
    if (!super())
        return -EPERM;

    // 找到要在哪一个目录下创建新目录，如果找不到或者没有写目录的权限，返回错误码。
    if (!(dir = dir_namei(pathname, &namelen, &basename)))
        return -ENOENT;
    if (!namelen)
    {
        iput(dir);
        return -ENOENT;
    }
    if (!permission(dir, MAY_WRITE))
    {
        iput(dir);
        return -EPERM;
    }

    // 如果该目录内已经存在了要创建的新目录项，则返回已经存在的错误码。
    bh = find_entry(&dir, basename, namelen, &de);
    if (bh)
    {
        brelse(bh);
        iput(dir);
        return -EEXIST;
    }

    // 创建新目录项的inode节点
    inode = new_inode(dir->i_dev);
    if (!inode)
    {
        iput(dir);
        return -ENOSPC;
    }
    inode->i_size = 32;        // 这个32是两个目录项的值，分别为.和.. ,这是不应该用32,而是写成2 * sizeof(struct dir_entry)更好。
    inode->i_dirt = 1;
    inode->i_mtime = CURRENT_TIME;
    inode->i_atime = CURRENT_TIME;

    // 为新的目录项inode节点创建一个数据块, 并在数据块中写入两个默认的目录项： . 和 .. 
    if (!(inode->i_zone[0] = new_block(inode->i_dev)))    // 得到的是逻辑块的索引号
    {
        iput(dir);
        inode->i_nlinks--;
        iput(inode);
        return -ENOSPC;
    }
    inode->i_dirt = 1;
    if (!(dir_block = bread(inode->i_dev, inode->i_zone[0])))
    {
        iput(dir);
        free_block(inode->i_dev, inode->i_zone[0]);
        inode->i_nlinks--;
        iput(inode);
        return -ERROR;
    }
    de = (struct dir_entry*)dir_block->b_data;
    de->inode = inode->i_num;
    strcpy(de->name, ".");

    ++de;
    de->inode = dir->i_num;
    strcpy(de->name, "..");
    inode->i_nlinks = 2;        // 为什么是2？？不明白！！只有目录 . 引用它了啊。
    dir_block->b_dirt = 1;
    brelse(dir_block);
    inode->i_mode = I_DIRECTORY | (mode & 0777 & ~current->umask);
    inode->i_dirt = 1;

    // 在上一级目录中创建新目录的目录项，把绑定到当前创建的inode节点上。
    bh = add_entry(dir, basename, nammelen, &de);
    if (!bh)
    {
        iput(dir);
        free_block(inode->i_dev, inode->i_zone[0]);
        inode->i_nlinks = 0;
        iput(inode);
        return -ENOSPC;
    }
    de->inode = inode->num;
    bh->b_dirt = 1;
    dir->i_nlinks++;
    dir->i_dirt = 1;
    iput(dir);
    iput(inode);
    brelse(bh);
    return 0;
}

/**
  @brief 该函数实现检测给定的目录内是否为空。
  @param [in] inode 给定的目录inode.
  @return 如果为空返回1, 如果不为空返回0.

  说明一下：一个目录的大小是它内部包含了多少个目录项，但是这些目录项可能没有全部被使用，
  如果目录项的inode为0,说明没有被使用，如果目录内的全部目录项没有被使用，说能说目录为空，
  而不是说目录内的目录项个数等于2(默认包含了 . 和 ..)才为空。 
 */
static int empty_dir(struct m_inode* inode)
{
    int nr, block;
    int len;
    struct buffer_head* bh;
    struct dir_entry* de;

    len = inode->i_size / sizeof(struct dir_entry);
    if (len < 2 || !inode->i_zone[0] || (!bh = bread(inode->i_dev, inode->i_zone[0])))
    {
        printk("warning: bad directory on dev %04x\n", inode->i_dev);
        return 0;
    }

    de = (struct dir_entry*)bh->data;
    // 检测一下.目录和..目录, 如果对应，打印错误信息，并返回。
    if (de[0].inode != inode->i_num || de[1].inode
            || strcmp(".", de[0].name) || strcmp("..", de[1].name))
    {
        printk("warning: bad directory on dev %04x\n", inode->i_dev);
        return 0;
    }

    /* 遍历检测目录内的目录项的inode是否全为0.如果有一个不为0时，就表示了该目录中不空。
       从代码中可以看出来，目录的大小是它里面包含了多少个目录项的大小，但是这些目录项不
       一定在使用，如果目录项没有使用，则目录项的inode为0. 
       有一个问题想知道：什么时候目录的size会减小呢？ */
    nr = 2;
    de += 2;
    while (nr < len)
    {
        // 当前的逻辑块读取完时，切换到下一下逻辑块中。
        if ((void*)de >= (void*)(bh->b_data + BLOCK_SIZE))
        {
            brelse(bh);
            if (!(block = bmap(inode, nr / DIR_ENTRIES_PER_BLOCK)))
            {
                nr += DIR_ENTRIES_PER_BLOCK;
                continue;
            }
            if (!(bh = bread(inode->i_dev, block)))
                return 0;
            de = (struct dir_entry*)bh->b_data;
        }

        // 判断目录项的inode是否为0
        if (de->inode)
        {
            brelse(bh);
            return 0;
        }
        de++;
        nr++;
    }
    brelse(bh);
    return 1;
}

/**
  @brief 系统调用： 删除给定的目录，它有很多限制条件来决定能否删除给定的目录。
  @param [in] name 待删除的目录路径名
  @return 删除成功返回0, 错误返回相应的错误码。
  */
int sys_rmdir(const char* name)
{
    const char* basename;
    int namelen;
    struct m_inode *dir, *inode;
    struct buffer_head* bh;
    struct dir_entry* de;

    if (!suser())
        return -EPERM;
    if (!(dir = dir_namei(name, &namelen, &basename)))
        return -ENOENT;

    // 不支持这样的路径： /etc/addf/adsf/ ， 则返回错误码。
    if (!namelen)
    {
        iput(dir);
        return -ENOENT;
    }
    // 没有办法写上一级目录的权限，就没有办法删除上一级目录下的当前目录。
    if (!permission(dir, MAY_WRITE))
    {
        iput(dir);
        return -EPERM;
    }
    // 如果找不到要删除目录的目录项，也返回错误码。
    if (!(bh = find_entry(&dir, basename, namelen, &de)))
    {
        iput(dir);
        return -ENOENT;
    }
    // 取目录项对应的inode节点，若出错，返回错误码。
    if (!inode = iget(dir->i_dev, de->inode))
    {
        iput(dir);
        brelse(bh);
        return -EPERM;
    }
    // 如果上一级的目录项设置了受限删除标志， 当前进程可能没有权限进行写操作
    if ((dir->i_mode & S_ISVTX) && current->euid && inode->i_uid != current->euid)
    {
        iput(dir);
        iput(inode);
        brelse(bh);
        return -EPERM;
    }
    // 如果要删除的目录的设备号与上一级目录的设备号不同，或者要删除的目录还在其它进程使用，则不能删除。
    if (inode->i_dev != dir->i_dev || inode->i_count > 1)
    {
        iput(dir);
        iput(inode);
        brelse(bh);
        return -EPERM;
    }
    // 此时，说明了要删除的目录为. , 不允许删除.  这里好好想想哦，没有错误。
    if (inode == dir)
    {
        iput(dir);
        iput(inode);
        brelse(bh);
        return -EPERM;
    }
    // 如果inode不是目录(可能是文件)，则不进行删除。
    if (!S_ISDIR(inode->i_mode))
    {
        iput(dir);
        iput(inode);
        brelse(bh);
        return -ENOTDIR;
    }
    // 如果目录不为空，也不能删除。
    if (!empty_dir(inode))
    {
        iput(dir);
        iput(inode);
        brelse(bh);
        return -ENOTEMPTY;
    }

    // 如果目录的链接数不为2,则给出警告信息。
    if (inode->i_nlinks != 2)
        printk("empty directory has nlinks != 2 (%d)", inode->i_nlinks);

    // 执行真正的删除相关操作。
    de->inode = 0;
    bh->b_dirt = 1;
    brelse(bh);
    inode->i_nlinks = 0;
    inode->i_dirt = 1;
    dir->i_nlinks--;
    dir->i_ctime = CURRENT_TIME;
    dir->i_mtime = CURRENT_TIME;
    dir->i_dirt = 1;
    iput(dir);
    iput(inode);
    return 0;
}

/**
  @brief  系统调用：删除指定的文件， 其实叫作解链接更合适, 因为一个inode可能关联了多个目录项。
  @param [in] name 要删除的文件路径名。
  @return 删除成功返回0, 如果失败返回相应的错误码。
  */
int sys_unlink(const char* name)
{
    const char* basename;
    int namelen;
    struct m_inode *dir, *inode;
    struct buffer_head* bh;
    struct dir_entry* de;

    // 获取上级目录的inode和要删除文件的文件名和文件名长度
    if (!(dir = dir_namei(name, &namelen, &basename)))
        return -ENOENT;
    if (!namelen)
    {
        iput(dir);
        return -ENOENT;
    }
    if (!permission(dir, MAY_WRITE))
    {
        iput(dir);
        return -EPERM;
    }

    // 获取要删除文件的目录项和目录项所在超级块的头指针
    if (!(bh = find_entry(&dir, basename, namelen, &de)))
    {
        iput(dir);
        return -ENOENT;
    }

    // 获取要删除文件的inode.
    if (!(inode = iget(dir->i_dev, de->inode)))
    {
        iput(dir);
        brelse(bh);
        return -ENOENT;
    }
    if((dir->i_mode & S_ISVTX)          // 不明白，为什么这里使用了逻辑与呢？？
        && !suser()
        && current->euid != inode->i_uid
        && current->euid != dir->i_uid)
    {
        iput(dir);
        iput(inode);
        brelse(bh);
        return -EPERM;
    }
    if (S_ISDIR(inode->i_mode))        // 不能删除目录
    {
        iput(dir);
        iput(inode);
        brelse(bh);
        return -EPERM;
    }
    if (!inode->i_nlinks)
    {
        printk("deleting nonexistent file");
        inode->i_nlinks = 1;         // 修改为1了， 与下面的具体删除操作有关。
    }

    // 执行具体的删除操作
    de->inode = 0;
    bh->b_dirt = 1;
    brelse(bh);
    inode->i_nlinks--;
    inode->i_dirt = 1;
    inode->i_ctime = CURRENT_TIME;
    iput(inode);
    iput(dir);
    return 0;
}

/**
  @brief  系统调用：该函数实现为一个文件创建一个硬链接, 不能为目录创建硬链接的。
  @param [in] oldname 原文件的路径文件名
  @param [in] newname 新文件的路径文件名
  @return 如果成功则返回0, 如果失败则返回相应的错误码。
  */
int sys_link(const char* oldname, const char* newname)
{
    struct dir_entry *de;
    struct m_inode *oldinode, *dir;
    struct buffer_head* bh;
    const char* basename;
    int namelen;

    // 获取原文件的inode
    if (!(oldinode = namei(oldname)))
        return -ENOENT;
    if (S_ISDIR(oldinode->i_mode))    // 不能为目录创建硬链接
    {
        iput(oldinode);
        return -EPERM;
    }

    // 获取新文件所在的目录inode和新文件名与新文件长度
    if (!(dir = dir_namei(newname, &namelen, &basename)))
    {
        iput(oldinode);
        return -EACCES;
    }
    if (!namelen)
    {
        iput(oldinode);
        iput(dir);
        return -EPERM;
    }
    if (!permission(dir, MAY_WRITE))
    {
        iput(oldinode);
        iput(dir);
        return -EACCES;
    }

    // 不同的设备块之间不可以建立硬链接的。
    if (dir->i_dev != oldinode->i_dev)
    {
        iput(oldinode);
        iput(dir);
        return -EXDEV;
    }

    /* 检测一下新文件名是否存在，如果存在的话，返回错误码, 如果不存在，则创建新文件
       对应的目录项。 */
    if (bh = find_entry(&dir, basename, namelen, &de))
    {
        brelse(bh);
        iput(dir);
        iput(oldinode);
        return -EEXIST;
    }
    if (!(bh = add_entry(dir, basename, namelen, &de)))
    {
        iput(dir);
        iput(oldinode);
        return -ENOSPC;
    }

    // 真正的文件创建动作
    de->inode = oldinode->i_num;
    bh->b_dirt = 1;
    brelse(bh);
    iput(dir);
    ++oldinode->i_nlinks;
    oldinode->i_ctime = CURRENT_TIME;
    oldinode->i_dirt = 1;
    iput(oldinode);
    return 0;
}
