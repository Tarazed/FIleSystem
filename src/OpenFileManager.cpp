#include "../include/OpenFileManager.h"
#include "../include/MyFsManager.h"

#include <time.h>
#include <stdio.h>


InodeTable::InodeTable()
{

}
InodeTable::~InodeTable()
{

}

void InodeTable::Initialize()
{
    this->fileSys = &MyFsManager::Instance().GetFileSystem();
}

Inode* InodeTable::IGet(int inumber)
{
    Inode *pInode;
    while(true)
    {
        /* 检查指定设备dev中编号为inumber的外存Inode是否有内存拷贝 */
        int index = this->IsLoaded(inumber);
        if(index>=0)//找到拷贝
        {
            pInode = &(this->m_Inode[index]);
            pInode->i_count++;
            pInode->i_flag |= Inode::ILOCK;
            return pInode;
        }
        else//需要分配空闲inode
        {
            pInode = this->GetFreeInode();
            /* 内存inode表已满 */
            if(pInode==NULL)
            {
                printf("Inode Table Overflow!!!\n");
                return NULL;
            }
            else//分配成功，读入外存inode
            {
                pInode->i_number = inumber;
                pInode->i_flag = Inode::ILOCK;
                pInode->i_count++;

                BufferManager& bufMgr = MyFsManager::Instance().GetBufferManager();
                Buf *pBuf = bufMgr.Bread(FileSystem::INODE_ZONE_START_SECTOR + inumber / FileSystem::INODE_NUMBER_PER_SECTOR);

                pInode->ICopy(pBuf,inumber);
                bufMgr.Brelse(pBuf);
                return pInode;
            }
        }
    }
    return NULL;
}

/* close文件时会调用Iput
 *      主要做的操作：内存i节点计数 i_count--；若为0，释放内存 i节点、若有改动写回磁盘
 * 搜索文件途径的所有目录文件，搜索经过后都会Iput其内存i节点。路径名的倒数第2个路径分量一定是个
 *   目录文件，如果是在其中创建新文件、或是删除一个已有文件；再如果是在其中创建删除子目录。那么
 *   	必须将这个目录文件所对应的内存 i节点写回磁盘。
 *   	这个目录文件无论是否经历过更改，我们必须将它的i节点写回磁盘。
 */
void InodeTable::IPut(Inode *pNode)
{
    pNode->IUpdate(time(NULL));
    if(pNode->i_count==1)//单进程暂时无意义
    {
        if(pNode->i_nlink<=0)//该文件已经没有目录路径指向它
        {
            pNode->ITrunc();
            pNode->i_mode=0;
            this->fileSys->IFree(pNode->i_number);
        }
        pNode->i_flag=0;
        pNode->i_number=-1;//表示空闲
        pNode->i_count--;
    }

}

void InodeTable::UpdateInodeTable()
{
    for(int i=0;i<InodeTable::NINODE;i++)
    {
        /* 如果Inode对象count不等于0，count == 0意味着该内存Inode未被任何打开文件引用，无需同步。*/
        if(this->m_Inode[i].i_count!=0)
        {
            this->m_Inode[i].IUpdate(time(NULL));
        }
    }
}

int InodeTable::IsLoaded(int inumber)
{
    for(int i=0;i<InodeTable::NINODE;i++)
    {
        if(this->m_Inode[i].i_number==inumber && this->m_Inode[i].i_count!=0)
            return i;
    }
    return -1;
}

Inode* InodeTable::GetFreeInode()
{
    for(int i=0;i<InodeTable::NINODE;i++)
    {
        if(this->m_Inode[i].i_count==0)
            return &(this->m_Inode[i]);
    }
    return NULL;
}
