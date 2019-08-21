#include <string.h>
#include <linux/sched.h>
#include <linux/kernel.h>

/**
  @brief 定义了一个内嵌汇编语言的宏函数， 作用是把给定地址的block大小的内存清空为零。
  @param [in] addr 给定的地址

  stosl汇编指令的作用是： store string, 把ax中的值存储到es:di处的内存空间中，并更新di的值。
  */
#define clear_block(addr)                                           \
    _asm__("cld\n\t"                                                \
            "rep\n\t"                                               \
            "stosl"                                                 \
            ::"a" (0), "c" (BLOCK_SIZE / 4), "D" ((long)(addr))     \
            : "cx", "di")

/**
  @brief 定义了一个内嵌汇编谎言的宏函数，实现的功能是：对一个long类型变量的指定位设置为1，并且返回原来的值(1或0).
  @param [in] nr 指定第几位。
  @param [in] addr 要设计的变量的地址。
  @return 返回0或1, 代表了你要设置的那个位原来的值。
  
  相关的汇编指令说明：  
  - btsl src, dest  把src索引号指定的dest上的目的位复制到CF标志位上，并把目的位置为1.
  - setb src  src为一个字节，如果CF位为1，则把src设置为1，如果CF位为0，则把src设置为0.
  */
#define set_bit(nr, addr) ({                                        \
register int res __asm__("ax");                                     \
__asm__ __volatile__("btsl %2, %3\n\t"                              \
                     "setb %%al"                                    \
                     :"=a"(res)                                     \
                     :"0"(0), "r" (nr), "m" (*(addr)));             \
res;})

/**
  @brief 定义了一个内嵌汇编谎言的宏函数，实现的功能是：对一个long类型变量的指定位设置为0，并且返回原来的值(1或0).
  @param [in] nr 指定第几位。
  @param [in] addr 要设计的变量的地址。
  @return 返回0或1, 代表了你要设置的那个位原来的值。
  
  相关的汇编指令说明：  
  - btrl src, dest  把src索引号指定的dest上的目的位复制到CF标志位上，并把目的位置为0.
  - setb src  src为一个字节，如果CF位为1，则把src设置为1，如果CF位为0，则把src设置为0.
  */
#define clear_bit(nr, addr) ({                                      \
register int res __asm__("ax");                                     \
__asm__ __volatile__("btrl %2, %3\n\t"                              \
                     "setb %%al"                                    \
                     :"=a"(res)                                     \
                     :"0"(0), "r" (nr), "m" (*(addr)));             \
res;})

/**
  @brief 该宏函数实现了从给定地址处查找第一个为0的位，返回查找到的下标，如果没有查找到，则返回8192.
  该宏函数只查找从addr处偏移到1kb的内存空间。
  @param [in] addr 给定的地址。
  @return 返回第一个为0的位的下标。
  
  涉及到的汇编指令包含：  
  - cld 指令：清空方向位DF， 使得一些字符串指令增加si和di的值。
  - lodsl指令：load string，把ds:si地址处的一个long类型变量放到eax寄存器中。
  - notl src 指令：对每一位进行逻辑
  - bsfl src des 指令： bit scan forward, 从src中查找（由0位至n位)查找第一个为1的位。如果
    查找到，则把ZF置1，并且把相应位的下标索引值放到des中；如果查找不到，ZF位则置为0.
  */
#define find_first_zero(addr) ({                                   \
int __res;                                                         \
__asm__("cld\n\t"                                                  \
        "1: lodsl \n\t"                                            \
        "notl %%eax\n\t"                                           \
        "bsfl %%eax, %%edx\n\t"                                    \
        "je 2f\n\t"                                                \
        "addl %%edx, %%ecx\n\t"                                    \
        "jmp 3f\n"                                                 \
        "2: addl $32, %%ecx\n\t"                                   \
        "cmpl $8192, %%ecx\n\t"                                    \
        "jl 1b\n"                                                  \
        "3:"                                                       \
        :"=c"(__res)                                               \
        :"c"(0), "S"(addr)                                         \
        :"ax", "dx", "si");                                        \
__res;})

/**
  @brief 释放给定指定设备上指定的逻辑块。要强调一下，它释放的是磁盘上的逻辑块啊。
  @param [in] dev 指定的设备
  @param [in] block 指定的逻辑块号
  @return 返回值为空。
  
  具体来说，它干了这么几件事：   
  1. 首先判断给定的dev号和block号是否合法，如果不合法，给出提示信息，并且死机！
  2. 接着，看看该逻辑块在高速缓冲区内是否有对应的缓冲区，如果存在，则释放掉内存中对应的高速缓冲块。
  3. 然后把超级块中的逻辑块映射对应的bit位清零。
  
  struct supper_block结构内，使用到的成员有：
  first_datazone, 第一个data块号。
  s_nzones: 这个到底是总的data块数呢？还是最后一个data块号啊？从代码上看，是最后一个data块号啊！但是书上说是总的块数！！！
  s_zmap[]:这是一个数组，里面存放了高速缓冲块头指针，使用高速缓冲块(每一个1024kb)的每一位映射一个逻辑块，用于标记该逻辑块是否被占用！
  */
void free_block(int dev, int block)
{
  struct supper_block *sb;
  struct buffer_head *bh;
  
  if (!(sb = get_super(dev)))
    panic("trying to free block on nonexistent device");
  if (block < sb->s_firstdatazone || block >= sb->s_nzones)    // s_nzones 表示了什么？
    panic("tring to free block not in datazone");
  
  bh = get_hash_table(dev, block);
  if (bh)
  {
    if (bh->b_count != 1)       // 为什么非得是1，万一大于1时，是什么情况？？还是说b_count要么是1，要么是0???
    {
      printk("trying to free block (%04x:%d), count = %d\n", dev, block, bh->b_count);
      return;    // 为什么直接return掉？超级块内的逻辑块对应的bit位还没有处理啊！！
    }
    bh->b_dirt = 0;
    bh_b_uptodate = 0;
    brelse(bh);
  }
  
  block -= sb->s_firsdatazone - 1;
  if (clear_bit(block & 8191, sb->s_zmap[block / 8192]->b_data))    // 这地方不对吧？逻辑是不是反了？
  {
    printk("block (%04x:%d)", dev, block+sb->s_firstdatazone - 1);
    panic("free_block: bit already cleared");
  }
  sb->s_zmap[block / 8192]->b_dirt = 1;
}

/**
  @brief 在指定设备上新申请一个block块，并把新的逻辑块执行清零操作。返回新申请的逻辑块号。
  @param [in] dev 给定的设备号。
  @return返回值为申请到的逻辑块号，如果没有申请到，则返回0.
  
  该函数作了如下事情：
  1. 从超级块中查找一个空闲的逻辑块，怎么查找呢？就是通过查找zmap中为0的位。
  2. 找到之后，在高速缓冲区建立一个对应[dev, block]的高速缓冲块，然后把高速缓冲块对应的数据区全部设置为0，
  然后把对应的b_dirt位置为1， b_uptodate位置为1，然后就可以释放该高速缓冲块了。 到底高速缓冲块内全为0的
  内容什么时候同步写入到磁盘中，肯定是别的程序来完成了(因为b_dirt位置为1， b_uptodate位置为1了)
  
  这代码写的的确牛逼！！！
  */
int new_block(int dev)
{
  struct buffer_head* bh;
  struct super_block *sb;
  int i;
  int j;
  
  if (!(sb = get_super(dev)))
    panic("trying to get new block from nonexistant device");
  
  j = 8192;
  for (i = 0; i < 8; i++)
  {
    if (bh = sb->s_zmap[i])
    {
      if ((j = find_first_zero(bh->b_data)) < 8192)
        break;
    }
  }
  
  if (i >=8 || !bh || j >= 8192)
    return 0;
  if (set_bit(j, bh->data))
    panic("new block: bit already set");
  
  bh->b_dirt = 1;
  j += i * 8192 + sb->s_firstdatazone - 1;
  if (j >= sb->s_nzones)
    return 0;
  if (!(bh = getblk(dev, j)))
    panic("new block: cannot get block");
  if (bh->b_count != 1)
    panic("new block: count is not equal 1");
  clear_block(bh->b_data);
  bh->b_uptodate = 1;
  bh->b_dirt = 1;
  brelse(bh); 
  return j;
}

/**
  @brief
  @param [in] inode 要释放的inode的指针
  @return 返回为空。
  
  干了两件事：  
  1. 判断inode指针以及inode指向的内容是否有效，如果无效则给出提示并死机。
  2. 把inode对应的超级块内的imap中的bit位清零，并且把inode指向的内存空间进行清零操作。
  
  使用到的struct m_inode成员变量有：  
  - i_dev: 
  - i_count:
  - i_nlinks: 链接到该inode上的目录项数目
  - i_num: 该inode节点的索引号
  
  */
void free_inode(struct m_inode* inode)
{
    struct super_block* sb;
    struct buffer_head* bh;
    
    if (!inode)
        return;
    if (!inode->i_dev)
    {
        memset(inode, 0, sizeof(*inode));
        return;
    }
    if (inode->i_count > 1)
    {
        printk("trying to free inode with count = %d\n", inode->i_count);
        panic("free inode");
    }
    if (inode->i_nlinks)
        panic("trying to free inode with links");
    if (!(sb = get_super(inode->i_dev)))
        panic("trying to free inode on nonexistent device");
    if (inode->i_num < 1 || inode->i_num > sb->s_ninodes)
        panic("trying to free inode 0 or nonexistent inode");
    if (!(bh = sb->s_imap[inode->i_num>>13]))
        panic("nonexistent imap in superblock");
    if (clear_bit(inode->i_num & 8191, bh->b_data))
        printk("free inode: bit already cleared\n\r");
    bh->b_dirt = 1;
    memset(inode, 0, sizeof(*inode));
}

/**
  @brief 该函数实现在指定设备上申请一个inode节点,返回申请到的inode节点的指针。
  @param [in] 指定的设备号
  @return 返回值为申请到的inode节点的指针,如果没有申请失败，则返回NULL.
  */
struct m_inode* new_inode(int dev)
{
  struct m_inode* inode;
  struct super_block* sb;
  struct buffer_head*bh;
  int i, j;
  
  if (!(inode = get_empty_inode()))
    return NULL;
  if (!(sb = get_super(dev)))
    panic("new inode with unknown device");
  
  j = 8192;
  for (i = 0; i < 8; ++i)
  {
    if (bh = sb->s_impa[i])
      if ((j = find_first_zero(bh->b_data)) < 8192)
        break;
  }
  if (!bh || j >= 8192 || j + i * 8192 > sb_s_ninodes)
  {
    iput(inode);
    return NULL;
  }
  
  if (set_
}
