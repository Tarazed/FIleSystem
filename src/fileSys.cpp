#include "../include/fileSys.h"
#include "../include/Buf.h"
#include "../include/MyFsManager.h"

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* SuperBlock类 */
SuperBlock::SuperBlock()
{

}
SuperBlock::~SuperBlock()
{

}

/* FileSystem类 */
FileSystem::FileSystem()
{
    this->g_spb = new SuperBlock();
}
FileSystem::~FileSystem()
{
    delete this->g_spb;
}

void FileSystem::Initialize()//初始化
{
	this->bufferManager = &MyFsManager::Instance().GetBufferManager();

    this->LoadSuperBlock();
}

void FileSystem::LoadSuperBlock()//读取SuperBlock
{
	Buf *pBuf;

	for(int i=0;i<2;i++)
	{
        int *p = (int *)this->g_spb + i * 128;
        pBuf = this->bufferManager->Bread(FileSystem::SUPER_BLOCK_SECTOR_NUMBER + i);
		memcpy(p,(int *)pBuf->b_addr,512);
        this->bufferManager->Brelse(pBuf);
	}
    this->g_spb->s_flock = 0;
    this->g_spb->s_ilock = 0;
    this->g_spb->s_ronly = 0;
    this->g_spb->s_time  = time(0);
}

void FileSystem::Update()//同步SuperBlock
{
    SuperBlock* sb = this->g_spb;
    Buf *pBuf;

    if(sb->s_fmod == 0 || sb->s_ilock != 0 || sb->s_flock != 0 || sb->s_ronly != 0)
    {
        return;
    }
    sb->s_fmod=0;
    sb->s_time = time(0);

    /*
     * 为将要写回到磁盘上去的SuperBlock申请一块缓存，由于缓存块大小为512字节，
     * SuperBlock大小为1024字节，占据2个连续的扇区，所以需要2次写入操作。
     */
    for(int i=0;i<2;i++)
    {
        /* 第一次p指向SuperBlock的第0字节，第二次p指向第512字节 */
        int* p = (int *)sb + i * 128;
        /* 将要写入到设备dev上的SUPER_BLOCK_SECTOR_NUMBER + j扇区中去 */
        pBuf = this->bufferManager->GetBlk(FileSystem::SUPER_BLOCK_SECTOR_NUMBER + i);
        memcpy((int *)pBuf->b_addr,p,512);
        this->bufferManager->Bwrite(pBuf);
    }
    //this->bufferManager->Bflush();
}

Inode* FileSystem::IAlloc()
{
    SuperBlock* sb = g_spb;
    Buf *pBuf;
    Inode *pNode;
    int inumber;//分配到的空闲外存inode编号

    if(sb->s_ninode<=0)//superblock直接管理的inode索引表已空
    {
        inumber = 0;//这里因为0号inode不用，所以从1开始
        for(int i=0;i<sb->s_isize;i++)
        {
            pBuf=this->bufferManager->Bread(FileSystem::INODE_ZONE_START_SECTOR+i);
            int *p = (int *)pBuf->b_addr;

            /* 检查该缓冲区中每个外存Inode的i_mode != 0，表示已经被占用 */
            for(int j=0;j<FileSystem::INODE_NUMBER_PER_SECTOR;j++)
            {
                inumber++;
                int mode = *(p+j*sizeof(DiskInode)/sizeof(int));
                /* 该外存inode已被占用，不能计入空闲inode索引表 */
                if(mode!=0)
                    continue;
                /*
                 * 如果外存inode的i_mode==0，此时并不能确定
                 * 该inode是空闲的，因为有可能是内存inode没有写到
                 * 磁盘上,所以要继续搜索内存inode中是否有相应的项
                 */
                if(MyFsManager::Instance().GetInodeTable().IsLoaded(inumber)==-1)
                {
                    sb->s_inode[sb->s_ninode++]=inumber;//内存没有对应拷贝
                    if(sb->s_ninode>=100)//空闲索引表已满，不继续搜索
                        break;
                }
            }
            this->bufferManager->Brelse(pBuf);
            if(sb->s_ninode>=100)//空闲索引表已满，不继续搜索
                break;
        }
        /* 如果磁盘上没有搜索到任何可用外存inode，返回NULL */
        if(sb->s_ninode<=0)
        {
            printf("No Space on Disk!!!\n");
            return NULL;
        }
    }
    /*
     * 上面部分已经保证，除非系统中没有可用外存Inode，
     * 否则空闲Inode索引表中必定会记录可用外存Inode的编号。
     */
    while(true)
    {
        inumber = sb->s_inode[--sb->s_ninode];
        pNode = MyFsManager::Instance().GetInodeTable().IGet(inumber);//空闲inode读入内存
        if(pNode==NULL)//未能分配到内存
        {
            printf("Inode Table Overflow!!!\n");
            return NULL;
        }
        if(pNode->i_mode==0)//清空空闲inode数据
        {
            pNode->Clean();
            sb->s_fmod=1;
            return pNode;
        }
        else
        {
            MyFsManager::Instance().GetInodeTable().IPut(pNode);
            continue;
        }
    }
    return NULL;
}

void FileSystem::IFree(int number)
{
    SuperBlock *sb = g_spb;
    if(sb->s_ninode>=100)
        return;
    sb->s_inode[sb->s_ninode++] = number;
    sb->s_fmod = 1;
}

Buf* FileSystem::BAlloc()
{
	int blkno;
    SuperBlock* sb = g_spb;
	Buf *pBuf;

	blkno = sb->s_free[--sb->s_nfree];
	if(blkno==0)//已满或者越界
	{
		sb->s_nfree = 0;
		printf("No Space!!!\n");
		return NULL;
	}

	/* 
	 * 栈已空，新分配到空闲磁盘块中记录了下一组空闲磁盘块的编号,
	 * 将下一组空闲磁盘块的编号读入SuperBlock的空闲磁盘块索引表s_free[100]中。
	 */
	if(sb->s_nfree<=0)
	{
		sb->s_flock++;
		pBuf = this->bufferManager->Bread(blkno);
		/* 从该磁盘块的0字节开始记录，共占据4(s_nfree)+400(s_free[100])个字节 */
		int* p = (int *)pBuf->b_addr;
		/* 首先读出空闲盘块数s_nfree */
		sb->s_nfree = *p++;
		/* 读取缓存中后续位置的数据，写入到SuperBlock空闲盘块索引表s_free[100]中 */
		memcpy(sb->s_free,p,400);
		this->bufferManager->Brelse(pBuf);
		sb->s_flock = 0;
	}
	/* 普通情况下成功分配到一空闲磁盘块 */
	pBuf = this->bufferManager->GetBlk(blkno);
	this->bufferManager->CLrBuf(pBuf);
	sb->s_fmod = 1;//SuperBlock被修改

	return pBuf;
}

void FileSystem::BFree(int blkno)
{
    SuperBlock* sb = g_spb;
	Buf* pBuf;

	sb->s_fmod = 1;
	/* 检查合法性 */
	if(blkno<FileSystem::DATA_ZONE_START_SECTOR || blkno>FileSystem::DATA_ZONE_END_SECTOR)
	{
		printf("Invalid Block Number!!!\n");
		return;
	}

	/* 
	 * 如果先前系统中已经没有空闲盘块，
	 * 现在释放的是系统中第1块空闲盘块
	 */
	if(sb->s_nfree <= 0)
	{
		sb->s_nfree = 1;
		sb->s_free[0] = 0;	/* 使用0标记空闲盘块链结束标志 */
	}
	/* SuperBlock中直接管理空闲磁盘块号的栈已满 */
	if(sb->s_nfree >= 100)
	{
		sb->s_flock = 1;
		/* 
		 * 使用当前BFree()函数正要释放的磁盘块，存放前一组100个空闲
		 * 磁盘块的索引表
		 */
		pBuf = this->bufferManager->GetBlk(blkno);	/* 为当前正要释放的磁盘块分配缓存 */
		/* 从该磁盘块的0字节开始记录，共占据4(s_nfree)+400(s_free[100])个字节 */
		int* p = (int *)pBuf->b_addr;
		
        /* 首先写入空闲盘块数，除了第一组为99块，后续每组都是100块 */
		*p++ = sb->s_nfree;
		/* 将SuperBlock的空闲盘块索引表s_free[100]写入缓存中后续位置 */
		memcpy(p,sb->s_free,400);
		sb->s_nfree=0;
		/* 将存放空闲盘块索引表的“当前释放盘块”写入磁盘，即实现了空闲盘块记录空闲盘块号的目标 */
		this->bufferManager->Bwrite(pBuf);

		sb->s_flock = 0;
	}
	sb->s_free[sb->s_nfree++] = blkno;	/* SuperBlock中记录下当前释放盘块号 */
	sb->s_fmod = 1;

}
