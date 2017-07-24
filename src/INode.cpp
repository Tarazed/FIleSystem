#include "../include/INode.h"
#include "../include/MyFsManager.h"

#include <string.h>
#include <stdio.h>
#include <algorithm>

Inode::Inode()
{
    /* 清空Inode对象中的数据 */
        // this->Clean();
        /* 去除this->Clean();的理由：
         * Inode::Clean()特定用于IAlloc()中清空新分配DiskInode的原有数据，
         * 即旧文件信息。Clean()函数中不应当清除i_dev, i_number, i_flag, i_count,
         * 这是属于内存Inode而非DiskInode包含的旧文件信息，而Inode类构造函数需要
         * 将其初始化为无效值。
         */
    /* 将Inode对象的成员变量初始化为无效值 */
        this->i_flag = 0;
        this->i_mode = 0;
        this->i_count = 0;
        this->i_nlink = 0;
        this->i_number = -1;
        this->i_uid = -1;
        this->i_gid = -1;
        this->i_size = 0;
        for(int i = 0; i < 10; i++)
        {
            this->i_addr[i] = 0;
        }
}

Inode::~Inode()
{

}

int Inode::Bmap(int lbn)
{
    Buf *pFirstBuf;
    Buf *pSecondBuf;
    int phyBlkno;   //转换后的物理盘块号
    int *iTable;    //用于访问索引盘中一次间接、两次间接索引表
    int index;
    BufferManager& bufMgr = MyFsManager::Instance().GetBufferManager();
    FileSystem& fileSys = MyFsManager::Instance().GetFileSystem();

    /*
     * Unix V6++的文件索引结构：(小型、大型和巨型文件)
     * (1) i_addr[0] - i_addr[5]为直接索引表，文件长度范围是0 - 6个盘块；
     *
     * (2) i_addr[6] - i_addr[7]存放一次间接索引表所在磁盘块号，每磁盘块
     * 上存放128个文件数据盘块号，此类文件长度范围是7 - (128 * 2 + 6)个盘块；
     *
     * (3) i_addr[8] - i_addr[9]存放二次间接索引表所在磁盘块号，每个二次间接
     * 索引表记录128个一次间接索引表所在磁盘块号，此类文件长度范围是
     * (128 * 2 + 6 ) < size <= (128 * 128 * 2 + 128 * 2 + 6)
     */
    if(lbn >= Inode::HUGE_FILE_BLOCK)
    {
        printf("Exceed of MAX FILE SIZE!!!\n");
        return 0;
    }

    if(lbn < 6)//小型文件
    {
        phyBlkno = this->i_addr[lbn];
        /*
         * 如果该逻辑块号还没有相应的物理盘块号与之对应，则分配一个物理块。
         * 这通常发生在对文件的写入，当写入位置超出文件大小，即对当前
         * 文件进行扩充写入，就需要分配额外的磁盘块，并为之建立逻辑块号
         * 与物理盘块号之间的映射。
         */
        if(phyBlkno==0 && (pFirstBuf=fileSys.BAlloc())!=NULL)
        {
            /*
            * 因为后面很可能马上还要用到此处新分配的数据块，所以不急于立刻输出到
            * 磁盘上；而是将缓存标记为延迟写方式，这样可以减少系统的I/O操作。
            */
            bufMgr.Bwrite(pFirstBuf);
            phyBlkno = pFirstBuf->b_blkno;
            /* 将逻辑块号映射到物理块号 */
            this->i_addr[lbn] = phyBlkno;
            this->i_flag |= Inode::IUPD;
        }
        return phyBlkno;
    }
    else//大型、巨型文件
    {
        /* 计算逻辑块号lbn对应的i_addr[]中的索引 */
        if(lbn<Inode::LARGE_FILE_BLOCK)// 大型文件: 长度介于7 - (128 * 2 + 6)个盘块之间
        {
            index = (lbn-Inode::SMALL_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK + 6;
        }
        else//巨型文件: 长度介于263 - (128 * 128 * 2 + 128 * 2 + 6)个盘块之间
        {
            index = (lbn - Inode::LARGE_FILE_BLOCK) / (Inode::ADDRESS_PER_INDEX_BLOCK * Inode::ADDRESS_PER_INDEX_BLOCK) + 8;
        }

        phyBlkno = this->i_addr[index];
        if(phyBlkno==0)//不存在相应的间接索引表块
        {
            this->i_flag |= Inode::IUPD;
            /* 分配空闲块存放间接索引表 */
            if((pFirstBuf=fileSys.BAlloc())==NULL)
            {
                return 0;//无空闲扇区
            }
            this->i_addr[index] = pFirstBuf->b_blkno;
        }
        else
            pFirstBuf = bufMgr.Bread(phyBlkno);
        /* 获取缓冲区首地址 */
        iTable = (int *)pFirstBuf->b_addr;
        if(index>=8)
        {
            /*
            * 对于巨型文件的情况，pFirstBuf中是二次间接索引表，
            * 还需根据逻辑块号，经由二次间接索引表找到一次间接索引表
            */
            index = ((lbn-Inode::LARGE_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK;

            /* iTable指向缓存中的二次间接索引表。该项为零，不存在一次间接索引表 */
            phyBlkno = iTable[index];

            if(phyBlkno==0)
            {
                unsigned char Buffer1[BufferManager::BUFFER_SIZE];
                memcpy(Buffer1,pFirstBuf->b_addr,BufferManager::BUFFER_SIZE);
                int tblkno1 = pFirstBuf->b_blkno;
                if((pSecondBuf=fileSys.BAlloc())==NULL)
                {
                    /* 分配一次间接索引表磁盘块失败，释放缓存中的二次间接索引表，然后返回 */
                    bufMgr.Brelse(pFirstBuf);
                    return 0;
                }
                unsigned char Buffer2[BufferManager::BUFFER_SIZE];
                memcpy(Buffer2,pSecondBuf->b_addr,BufferManager::BUFFER_SIZE);
                int tblkno2 = pSecondBuf->b_blkno;

                iTable = (int *)&Buffer1;
                iTable[index] = pSecondBuf->b_blkno;
                memcpy(pFirstBuf->b_addr,Buffer1,BufferManager::BUFFER_SIZE);
                pFirstBuf->b_blkno=tblkno1;
                bufMgr.Bwrite(pFirstBuf);

                memcpy(pSecondBuf->b_addr,Buffer2,BufferManager::BUFFER_SIZE);
                pSecondBuf->b_blkno = tblkno2;
                pFirstBuf = pSecondBuf;
                iTable = (int *)pSecondBuf->b_addr;//指向一次间接索引表
            }
            else
            {
                /* 释放二次间接索引表占用的缓存，并读入一次间接索引表 */
                bufMgr.Brelse(pFirstBuf);
                pSecondBuf = bufMgr.Bread(phyBlkno);

                pFirstBuf = pSecondBuf;
                iTable = (int *)pSecondBuf->b_addr;//指向一次间接索引表
            }

        }
        /* 计算逻辑块号lbn最终位于一次间接索引表中的表项序号index */
        if(lbn<Inode::LARGE_FILE_BLOCK)
            index = (lbn-Inode::SMALL_FILE_BLOCK)%Inode::ADDRESS_PER_INDEX_BLOCK;
        else
            index = (lbn-Inode::LARGE_FILE_BLOCK)%Inode::ADDRESS_PER_INDEX_BLOCK;

        unsigned char Buffer[BufferManager::BUFFER_SIZE];
        memcpy(Buffer,pFirstBuf->b_addr,BufferManager::BUFFER_SIZE);
        int tblkno = pFirstBuf->b_blkno;
        if((phyBlkno=iTable[index])==0 && (pSecondBuf=fileSys.BAlloc())!=NULL)
        {
            /* 将分配到的文件数据盘块号登记在一次间接索引表中 */
            phyBlkno = pSecondBuf->b_blkno;
            /* 将数据盘块、更改后的一次间接索引表输出到磁盘 */
            bufMgr.Bwrite(pSecondBuf);
            memcpy(pFirstBuf->b_addr,Buffer,BufferManager::BUFFER_SIZE);
            iTable[index] = phyBlkno;
            pFirstBuf->b_blkno=tblkno;
            bufMgr.Bwrite(pFirstBuf);
        }
        else
        {
            memcpy(pFirstBuf->b_addr,Buffer,BufferManager::BUFFER_SIZE);
            pFirstBuf->b_blkno=tblkno;
            bufMgr.Brelse(pFirstBuf);//释放一次间接索引表占用缓存
        }
        return phyBlkno;
    }
}

void Inode::IUpdate(int time)
{
    Buf *pBuf;
    DiskInode diskInode;
    FileSystem& fileSys = MyFsManager::Instance().GetFileSystem();
    BufferManager& bufMgr = MyFsManager::Instance().GetBufferManager();

    /* 当IUPD和IACC标志之一被设置，才需要更新相应DiskInode
     * 目录搜索，不会设置所途径的目录文件的IACC和IUPD标志 */
    if((this->i_flag & (Inode::IUPD | Inode::IACC))!=0)
    {
        if(fileSys.g_spb->s_ronly != 0)
            return;//只读文件系统
        pBuf = bufMgr.Bread(FileSystem::INODE_ZONE_START_SECTOR + this->i_number / FileSystem::INODE_NUMBER_PER_SECTOR);

        diskInode.d_mode = this->i_mode;
        diskInode.d_nlink = this->i_nlink;
        diskInode.d_uid = this->i_uid;
        diskInode.d_gid = this->i_gid;
        diskInode.d_size = this->i_size;
        for(int i=0;i<10;i++)
            diskInode.d_addr[i] = this->i_addr[i];
        diskInode.d_mtime = time;

        /* 将p指向缓存区中旧的外存inode的偏移位置 */
        unsigned char* p = pBuf->b_addr+(this->i_number%FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
        DiskInode* pNode = &diskInode;
        memcpy((int *)p,(int *)pNode,sizeof(DiskInode));
        bufMgr.Bwrite(pBuf);
    }
}

void Inode::ITrunc()
{
    FileSystem& fileSys = MyFsManager::Instance().GetFileSystem();
    BufferManager& bufMgr = MyFsManager::Instance().GetBufferManager();
    /* 采用FILO方式释放，以尽量使得SuperBlock中记录的空闲盘块号连续。
     *
     * Unix V6++的文件索引结构：(小型、大型和巨型文件)
     * (1) i_addr[0] - i_addr[5]为直接索引表，文件长度范围是0 - 6个盘块；
     *
     * (2) i_addr[6] - i_addr[7]存放一次间接索引表所在磁盘块号，每磁盘块
     * 上存放128个文件数据盘块号，此类文件长度范围是7 - (128 * 2 + 6)个盘块；
     *
     * (3) i_addr[8] - i_addr[9]存放二次间接索引表所在磁盘块号，每个二次间接
     * 索引表记录128个一次间接索引表所在磁盘块号，此类文件长度范围是
     * (128 * 2 + 6 ) < size <= (128 * 128 * 2 + 128 * 2 + 6)
     */
    for(int i=9;i>=0;i--)
    {
        if(this->i_addr[i]!=0)
        {
            if(i>=6&&i<=9)
            {
                Buf* pFirstBuf = bufMgr.Bread(this->i_addr[i]);
                int* pFirst = (int *)pFirstBuf->b_addr;
                unsigned char Buffer1[BufferManager::BUFFER_SIZE];
                memcpy(Buffer1,pFirstBuf->b_addr,BufferManager::BUFFER_SIZE);
                int tblkno1 = pFirstBuf->b_blkno;
                /* 每张间接索引表记录 512/sizeof(int) = 128个磁盘块号，遍历这全部128个磁盘块 */
                for(int j=128-1;j>=0;j--)
                {
                    if(pFirst[j]!=0)
                    {
                        if(i>=8&&i<=9)
                        {
                            Buf* pSecondBuf = bufMgr.Bread((pFirst[j]));
                            int* pSecond = (int *)pSecondBuf->b_addr;
                            for(int k=128-1;k>=0;k--)
                            {
                                if(pSecond[k]!=0)
                                    fileSys.BFree(pSecond[k]);
                            }
                            bufMgr.Brelse(pSecondBuf);
                        }//end if
                        memcpy(pFirstBuf->b_addr,Buffer1,BufferManager::BUFFER_SIZE);
                        pFirstBuf->b_blkno=tblkno1;
                        pFirst = (int *)&Buffer1;
                        fileSys.BFree(pFirst[j]);
                    }//end if
                }//end for
                bufMgr.Brelse(pFirstBuf);
            }//end if
            fileSys.BFree(this->i_addr[i]);
            this->i_addr[i] = 0;//0表示该索引不存在
        }
    }

    /* 盘块释放完毕，文件大小清零 */
    this->i_size = 0;
    /* 增设IUPD标志位，表示此内存Inode需要同步到相应外存Inode */
    this->i_flag |= Inode::IUPD;
    /* 清大文件标志 和原来的RWXRWXRWX比特*/
    this->i_mode &= ~(Inode::ILARG & Inode::IRWXU & Inode::IRWXG & Inode::IRWXO);
    this->i_nlink = 1;
}

void Inode::Clean()
{
    /*
     * Inode::Clean()特定用于IAlloc()中清空新分配DiskInode的原有数据，
     * 即旧文件信息。Clean()函数中不应当清除i_dev, i_number, i_flag, i_count,
     * 这是属于内存Inode而非DiskInode包含的旧文件信息，而Inode类构造函数需要
     * 将其初始化为无效值。
     */
    this->i_mode = 0;
    this->i_nlink = 0;
    this->i_uid = -1;
    this->i_gid = -1;
    this->i_size = 0;
    for(int i=0;i<10;i++)
        this->i_addr[i]=0;
}

void Inode::ICopy(Buf *bp, int inumber)
{
    DiskInode diskInode;
    DiskInode* pNode = &diskInode;

    unsigned char* p = bp->b_addr + (inumber%FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
    memcpy((int *)pNode,(int *)p,sizeof(DiskInode));

    /* 将外存Inode变量dInode中信息复制到内存Inode中 */
    this->i_mode = diskInode.d_mode;
    this->i_nlink = diskInode.d_nlink;
    this->i_uid = diskInode.d_uid;
    this->i_gid = diskInode.d_gid;
    this->i_size = diskInode.d_size;
    for(int i = 0; i < 10; i++)
    {
        this->i_addr[i] = diskInode.d_addr[i];
    }

}

void Inode::ReadI()
{
    int lbn;        //逻辑块号
    int phyBlkno;   //物理块号
    int offset;     //当前字符块内起始传送位置
    int nbytes;     //传送至用户目标区字节数量
    Buf* pBuf;
    BufferManager& bufMgr = MyFsManager::Instance().GetBufferManager();
    FileManager& fileMgr = MyFsManager::Instance().GetFileManager();

    if(fileMgr.m_Count==0)
        return ;
    this->i_flag |= Inode::IACC;

    /* 一次一个字符块地读入所需全部数据，直至文件尾 */
    while(fileMgr.m_Count!=0)
    {
        lbn = phyBlkno = fileMgr.m_Offset/Inode::BLOCK_SIZE;
        offset = fileMgr.m_Offset % Inode::BLOCK_SIZE;
        /* 传送到用户区的字节数量，取读请求的剩余字节数与当前字符块内有效字节数较小值 */
        nbytes = std::min(Inode::BLOCK_SIZE-offset,fileMgr.m_Count);
        int remain = this->i_size - fileMgr.m_Offset;
        if(remain<=0)//读到文件尾
            return ;
        nbytes = std::min(nbytes,remain);
        if((phyBlkno=this->Bmap(lbn))==0)
            return ;
        pBuf = bufMgr.Bread(phyBlkno);
        unsigned char* start = pBuf->b_addr + offset;//缓存中数据起始位置
        memcpy(fileMgr.m_Base,start,nbytes);

        /* 更新读写位置 */
        fileMgr.m_Base += nbytes;
        fileMgr.m_Offset += nbytes;
        fileMgr.m_Count -= nbytes;

        bufMgr.Brelse(pBuf);
    }
}

void Inode::WriteI()
{
    int lbn;        //逻辑块号
    int phyBlkno;   //物理块号
    int offset;     //当前字符块内起始传送位置
    int nbytes;     //传送至用户目标区字节数量
    Buf* pBuf;
    BufferManager& bufMgr = MyFsManager::Instance().GetBufferManager();
    FileManager& fileMgr = MyFsManager::Instance().GetFileManager();

    this->i_flag |= (Inode::IACC | Inode::IUPD);

    if(fileMgr.m_Count==0)
        return ;

    while(fileMgr.m_Count!=0)
    {
        lbn = fileMgr.m_Offset / Inode::BLOCK_SIZE;
        offset = fileMgr.m_Offset % Inode::BLOCK_SIZE;
        nbytes = std::min(Inode::BLOCK_SIZE-offset,fileMgr.m_Count);

        if((phyBlkno=this->Bmap(lbn))==0)
            return ;

        if(nbytes==Inode::BLOCK_SIZE)//如果数据刚好够一个字符块，则为其分配缓存
            pBuf=bufMgr.GetBlk(phyBlkno);
        else//不满一个字符块，先读后写（读出该字符块以保护不需要重写的数据）
            pBuf=bufMgr.Bread(phyBlkno);

        unsigned char* start = pBuf->b_addr + offset;
        memcpy(start,fileMgr.m_Base,nbytes);

        /* 更新读写位置 */
        fileMgr.m_Base += nbytes;
        fileMgr.m_Offset += nbytes;
        fileMgr.m_Count -= nbytes;

        if((fileMgr.m_Offset%Inode::BLOCK_SIZE)==0)//写满一个字符块
            bufMgr.Bwrite(pBuf);
        else
            bufMgr.Bwrite(pBuf);

        //文件长度增加
        if(this->i_size<fileMgr.m_Offset)
        {
            this->i_size = fileMgr.m_Offset;
        }
        this->i_flag |= Inode::IUPD;
    }
}

DiskInode::DiskInode()
{
    this->d_mode = 0;
    this->d_nlink = 0;
    this->d_uid = -1;
    this->d_gid = -1;
    this->d_size = 0;
    for(int i = 0; i < 10; i++)
    {
        this->d_addr[i] = 0;
    }
    this->d_atime = 0;
    this->d_mtime = 0;
}
DiskInode::~DiskInode()
{

}
