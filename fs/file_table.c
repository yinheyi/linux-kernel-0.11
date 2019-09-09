#include <linux/fs.h>

struct file file_table[NR_FILE];

/*
NR_FILE的值为64， file的结构如下所示： 
struct file
{
    unsigned short f_mode;
    unsigned short f_flags;
    unsigned short f_count;
    struct m_inode* f_inode;
    off_t f_pos;
};
*/
