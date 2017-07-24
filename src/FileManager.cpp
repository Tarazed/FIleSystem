#include "../include/FileManager.h"
#include "../include/MyFsManager.h"
#include "../include/DirectoryEntry.h"

#include "stdio.h"
#include "string.h"
#include <iostream>

FileManager::FileManager()
{

}
FileManager::~FileManager()
{

}

void FileManager::Initialize(bool flag)
{
    this->m_FileSystem = &MyFsManager::Instance().GetFileSystem();
    this->m_InodeTable = &MyFsManager::Instance().GetInodeTable();

    /* 获取rootDirInode:0#inode */
    this->rootDirInode = MyFsManager::Instance().GetInodeTable().IGet(0);
    if(flag==true)
    {
        /* 初始化根节点目录项，包含.和.. */
        Buf *pBuf = MyFsManager::Instance().GetFileSystem().BAlloc();
        this->rootDirInode->i_addr[0] = pBuf->b_blkno;
        this->rootDirInode->i_size += 2*sizeof(DirectoryEntry);
        DirectoryEntry dir[2];
        dir[0].m_name[0]='.';
        for(int i=1;i<DirectoryEntry::DIRSIZE;i++)
            dir[0].m_name[i]='\0';

        dir[0].m_inumber=this->rootDirInode->i_number;
        dir[1].m_name[0]='.';
        dir[1].m_name[1]='.';
        for(int i=2;i<DirectoryEntry::DIRSIZE;i++)
            dir[1].m_name[i]='\0';
        dir[1].m_inumber=this->rootDirInode->i_number;
        memcpy(pBuf->b_addr,dir,sizeof(dir));
        MyFsManager::Instance().GetBufferManager().Bwrite(pBuf);
        this->rootDirInode->i_flag |= Inode::IUPD;
        MyFsManager::Instance().GetInodeTable().IPut(this->rootDirInode);
    }


    /* 获取rootDirInode:0#inode */
    this->rootDirInode = MyFsManager::Instance().GetInodeTable().IGet(0);
    this->cdir = MyFsManager::Instance().GetInodeTable().IGet(0);
    this->pdir = MyFsManager::Instance().GetInodeTable().IGet(0);
    strcpy(this->curdir,"/");
}

int FileManager::fcreat(const char *name, int mode)//创建文件或目录
{
    Inode* pInode;
    bool flag;//用于判断是否找到
    pInode = this->NameI(name,FileManager::CREATE,flag);
    if(NULL == pInode && flag==true)
    {
        pInode=this->MakeNode(mode);
        if(NULL == pInode)
        {
            printf("Create Failed!!!\n");
            return -1;
        }
    }
    else if(flag==true)
    {
        pInode->ITrunc();
    }
    return 0;
}

Inode* FileManager::fopen(const char *name)//打开文件
{
    Inode* pInode;
    bool flag;
    pInode = this->NameI(name,FileManager::OPEN,flag);
    return pInode;
}

void FileManager::fclose()
{
    this->m_Count=0;
    this->m_Offset=0;
    this->m_Base=NULL;
    this->f_Offset=0;
    //pInode->i_flag |= (Inode::IACC | Inode::IUPD);
    //this->m_InodeTable->IPut(pInode);

}

int FileManager::fread(Inode* pInode,char *buf, int length)
{
    this->m_Base = (unsigned char*)buf;
    this->m_Count = length;
    this->m_Offset = this->f_Offset;
    pInode->ReadI();
    this->f_Offset = this->m_Offset;
    return length-this->m_Count;
}

int FileManager::fwrite(Inode *pInode, char *buf, int length)
{
    this->m_Base = (unsigned char*)buf;
    this->m_Count = length;
    this->m_Offset = this->f_Offset;
    pInode->WriteI();
    this->f_Offset = this->m_Offset;
    pInode->i_flag |= (Inode::IACC | Inode::IUPD);

    return length-this->m_Count;
}

void FileManager::fdelete(const char *name)
{
    Inode* pInode;
    bool flag;
    pInode=this->NameI(name,FileManager::DELETE,flag);
    if(pInode!=NULL)//返回了父目录
    {
        int i;
        char iname[DirectoryEntry::DIRSIZE];
        memset(iname,0,sizeof(iname));
        for(i=strlen(name)-1;name[i]!='/'&&i>=0;i--)
            ;
        if(i<0)
            i++;
        strcpy(iname,name+i);
        DirectoryEntry tmpdir;
        int count;
        while((count = this->fread(pInode,(char *)&tmpdir,32))!=0)
        {
            if(strcmp(tmpdir.m_name,iname)==0)//找到要删除的目录项
            {
                Inode* tmpnode;
                tmpnode = this->m_InodeTable->IGet(tmpdir.m_inumber);
                tmpnode->ITrunc();
                MyFsManager::Instance().GetFileSystem().IFree(tmpdir.m_inumber);
                memset(&tmpdir,0,sizeof(tmpdir));
                this->m_Offset-=sizeof(DirectoryEntry);
                this->m_Count=sizeof(DirectoryEntry);//DirectoryEntry::DIRSIZE + 4;
                this->m_Base=(unsigned char*)&tmpdir;

                pInode->WriteI();//目录项写入父目录
                this->m_InodeTable->IPut(pInode);
                break;
            }
        }
    }
}

char* simplifyPath(char* path)
{
    int top = -1;
    int i;
    int j;

    for(i = 0; path[i] != '\0'; ++i)
    {
        path[++top] = path[i];
        if(top >= 1 && path[top - 1] == '/' && path[top] == '.' && (path[i + 1] == '/' || path[i + 1] == '\0'))
        {
            top -= 2;
        }
        else if(top >= 2 && path[top - 2] == '/' && path[top - 1] == '.' && path[top] == '.' && (path[i + 1] == '/' || path[i + 1] == '\0'))
        {
            for(j = top - 3; j >= 0; --j)
            {
                if(path[j] == '/') break;
            }
            if(j < 0)
            {
                top = -1;
            }
            else
            {
                top = j - 1;
            }
        }
        else if(path[top] == '/' && path[i + 1] == '/') --top;
    }
    if(top > 0)
    {
        if(path[top] == '/') path[top] = '\0';
        else path[top + 1] = '\0';
    }
    else if(top == 0) path[top + 1] = '\0';
    else
    {
        path[0] = '/';
        path[1] = '\0';
    }
    return path;
}

void FileManager::ChDir(const char *pathname)
{
    Inode* pInode;
    bool flag;
    pInode = this->NameI(pathname,FileManager::OPEN,flag);
    if(pInode == NULL)
    {
        //printf("Dir not found!!!\n");
        return;
    }
    if((pInode->i_mode & Inode::IFMT)!=Inode::IFDIR)
    {
        this->m_InodeTable->IPut(pInode);
        printf("%s is not a dir!!!\n",pathname);
        return;
    }
    this->m_InodeTable->IPut(this->cdir);
    this->cdir = pInode;

    char* newpathname = (char *)pathname;
    newpathname = simplifyPath(newpathname);
    this->SetCurDir(pathname);
}

void FileManager::ls()
{
    DirectoryEntry tmp;
    int count;
    this->f_Offset=0;
    while((count = this->fread(this->cdir,(char *)&tmp,32))!=0)
    {
        if(tmp.m_inumber>0||tmp.m_name[0]=='.')
            printf("%s\t",tmp.m_name);
    }
    printf("\n");
    this->fclose();
}

/* 返回NULL表示目录搜索失败，否则是根指针，指向文件的内存打开inode */
Inode* FileManager::NameI(const char *pathname, DirectorySearchMode mode, bool& flag)
{
    Inode* pInode;
    Buf* pBuf;
    char curchar;
    char* pChar;
    int freeEntryOffset;    //以创建文件方式搜索目录时，记录空闲目录项的偏移量
    BufferManager& bufMgr = MyFsManager::Instance().GetBufferManager();

    flag=true;

    /*
     * 如果该路径是'/'开头的，从根目录开始搜索，
     * 否则从进程当前工作目录开始搜索。
     */

    if('/'==(curchar=*pathname++))
        pInode = this->rootDirInode;
    else
        pInode = this->cdir;

    /* 检查该Inode是否正在被使用，以及保证在整个目录搜索过程中该Inode不被释放 */
    this->m_InodeTable->IGet(pInode->i_number);

    while ( '/' == curchar )
    {
        curchar = *pathname++;
    }

    /* 如果试图更改和删除当前目录文件则出错 */
    if('\0'==curchar && mode!=FileManager::OPEN)
    {
        printf("Not allowed to change or delete curdir!!!\n");
        this->m_InodeTable->IPut(pInode);
        return NULL;
    }

    /* 外层循环每次处理pathname中一段路径分量 */
    while(true)
    {
        /* 出错则释放当前搜索到的目录文件Inode，并退出 */
        //

        /* 整个路径搜索完毕，则返回相应inode指针 */
        if('\0'==curchar)
            return pInode;

        /* 如果要进行搜索的不是目录文件，释放相关Inode资源则退出 */
        if((pInode->i_mode & Inode::IFMT) != Inode::IFDIR)
        {
            printf("Not dir!!!\n");
            break;
        }

        /* 将Pathname中当前准备进行匹配的路径分量拷贝到dbuf[]中，便于和目录项进行比较 */
        pChar = &(this->dbuf[0]);
        while('/'!=curchar && '\0'!=curchar)
        {
            if(pChar < &(this->dbuf[DirectoryEntry::DIRSIZE]))
            {
                *pChar = curchar;
                pChar++;
            }
            curchar=*pathname++;
        }
        /* 将dbuf剩余的部分填充为'\0' */
        while(pChar < &(this->dbuf[DirectoryEntry::DIRSIZE]))
        {
            *pChar = '\0';
            pChar++;
        }
        /* 允许出现////a//b 这种路径 这种路径等价于/a/b */
        while ( '/' == curchar )
        {
            curchar = *pathname++;
        }

        /* 内层循环部分对于dbuf[]中的路径名分量，逐个搜寻匹配的目录项 */
        this->m_Offset = 0;
        /* 设置为目录项个数，含空白目录项 */
        this->m_Count = pInode->i_size / (DirectoryEntry::DIRSIZE + 4);
        freeEntryOffset = 0;
        pBuf = NULL;
        while(true)
        {
            if(this->m_Count==0)//对目录项已经搜索完
            {
                if(pBuf!=NULL)
                    bufMgr.Brelse(pBuf);
                /* if create */
                if(FileManager::CREATE == mode && curchar=='\0')
                {
                    /* 将父目录inode指针保存起来，以后写目录项WriteDir()函数会用到 */
                    this->pdir = pInode;

                    if(freeEntryOffset)
                        this->m_Offset = freeEntryOffset - sizeof(DirectoryEntry);//(DirectoryEntry::DIRSIZE +  4);
                    else
                        pInode->i_flag |= Inode::IUPD;
                    /* 找到可以写入的空闲目录项位置，NameI()函数返回 */
                    return NULL;
                }
                /* 目录项搜索完毕而没有找到匹配项，释放相关Inode资源，并退出 */
                this->m_InodeTable->IPut(pInode);
                flag=false;
                printf("Dir not found!!!\n");
                return NULL;
            }
            /* 已读完目录文件的当前盘块，需要读入下一目录项数据盘块 */
            if(this->m_Offset%Inode::BLOCK_SIZE == 0)
            {
                if(pBuf!=NULL)
                    bufMgr.Brelse(pBuf);
                /* 计算要读的物理盘块号 */
                int phyBlkno = pInode->Bmap(this->m_Offset/Inode::BLOCK_SIZE);
                pBuf = bufMgr.Bread(phyBlkno);
            }
            /* 没有读完当前目录项盘块，则读取下一目录项至dent */
            int *src = (int *)(pBuf->b_addr+(this->m_Offset%Inode::BLOCK_SIZE));
            memcpy((int *)&this->dent,src,sizeof(DirectoryEntry));

            this->m_Offset+=sizeof(DirectoryEntry);//(DirectoryEntry::DIRSIZE + 4);
            this->m_Count--;
            /* 如果是空闲目录项，记录该项位于目录文件中偏移量 */
            if(this->dent.m_inumber==0 && this->m_Offset>64)
            {
                if(freeEntryOffset==0)
                    freeEntryOffset=this->m_Offset;
                continue;
            }
            int i;
            for(i=0;i<DirectoryEntry::DIRSIZE;i++)
            {
                if(this->dbuf[i]!=this->dent.m_name[i])
                    break;// 匹配至某一字符不符，跳出for循环
            }
            if(i<DirectoryEntry::DIRSIZE)// 不是要搜索的目录项，继续匹配下一目录项
                continue;
            else//目录项匹配成功，回到外层While(true)循环
                break;
        }
        /*
         * 从内层目录项匹配循环跳至此处，说明pathname中
         * 当前路径分量匹配成功了，还需匹配pathname中下一路径
         * 分量，直至遇到'\0'结束。
         */
        if(pBuf!=NULL)
            bufMgr.Brelse(pBuf);
        /* 如果是删除操作，则返回父目录inode，而要删除文件的inode号在dent.m_inumber中 */
        if(FileManager::DELETE == mode && '\0'==curchar)
        {
            return pInode;
        }
        /*
         * 匹配目录项成功，则释放当前目录inode，根据匹配成功的
         * 目录项m_inumber字段获取相应下一级目录或文件的inode。
         */
        this->m_InodeTable->IPut(pInode);
        pInode = this->m_InodeTable->IGet(this->dent.m_inumber);
        /* 回到外层While(true)循环，继续匹配Pathname中下一路径分量 */

        if ( pInode==NULL )	/* 获取失败 */
        {
            return NULL;
        }
    }
    this->m_InodeTable->IPut(pInode);
    return NULL;
}

/* 由fcreat调用 */
Inode* FileManager::MakeNode(unsigned int mode)
{
    Inode* pInode;
    pInode = this->m_FileSystem->IAlloc();
    if(pInode==NULL)
        return NULL;
    pInode->i_flag |= (Inode::IACC | Inode::IUPD);
    pInode->i_mode = mode | Inode::IALLOC;
    pInode->i_nlink = 1;
    pInode->i_uid = -1;
    pInode->i_gid = -1;
    this->WriteDir(pInode);
    return pInode;
}

void FileManager::WriteDir(Inode *pInode)
{
    if((pInode->i_mode & Inode::IFMT) == Inode::IFDIR)//创建目录需要分配新的目录文件
    {
        /* 初始化目录项，包含.和.. */
        Buf *pBuf = MyFsManager::Instance().GetFileSystem().BAlloc();
        pInode->i_addr[0] = pBuf->b_blkno;
        pInode->i_size += 2*sizeof(DirectoryEntry);
        DirectoryEntry dir[2];
        dir[0].m_name[0]='.';
        for(int i=1;i<DirectoryEntry::DIRSIZE;i++)
            dir[0].m_name[i]='\0';

        dir[0].m_inumber=pInode->i_number;//当前目录inode就是自己
        dir[1].m_name[0]='.';
        dir[1].m_name[1]='.';
        for(int i=2;i<DirectoryEntry::DIRSIZE;i++)
            dir[1].m_name[i]='\0';
        dir[1].m_inumber=this->pdir->i_number;//父目录inode
        memcpy(pBuf->b_addr,dir,sizeof(dir));
        MyFsManager::Instance().GetBufferManager().Bwrite(pBuf);
    }

    this->dent.m_inumber = pInode->i_number;
    for(int i=0;i<DirectoryEntry::DIRSIZE;i++)
        this->dent.m_name[i] = this->dbuf[i];
    this->m_Count=sizeof(DirectoryEntry);//DirectoryEntry::DIRSIZE + 4;
    this->m_Base=(unsigned char*)&this->dent;

    this->pdir->WriteI();//目录项写入父目录
    this->m_InodeTable->IPut(this->pdir);
    this->m_InodeTable->IPut(pInode);
}

void FileManager::SetCurDir(const char *pathname)
{
    if(strcmp(pathname,".")==0)//当前目录
        return;
    else if(strcmp(pathname,"..")==0)//父目录
    {
        int length = strlen(this->curdir);
        if(this->curdir[length-1]!='/')
        {
            int i;
            for(i=length-1;curdir[i]!='/'&&i>=0;i--)
                ;
            char *tmpdir = new char[50];
            strcpy(tmpdir,this->curdir);
            strncpy(this->curdir,tmpdir,i+1);
            this->curdir[i+1]='\0';
            delete tmpdir;
        }
    }
    /* 路径不是从根目录'/'开始，则在现有curdir后面加上当前路径分量 */
    else if(pathname[0]!='/')
    {
        int length = strlen(this->curdir);
        if(this->curdir[length-1]!='/')
        {
            this->curdir[length] = '/';
            length++;
        }
        strcpy(this->curdir+length,pathname);
    }
    else/* 如果是从根目录'/'开始，则取代原有工作目录 */
        strcpy(this->curdir,pathname);
}
