#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "fileSys.h"
#include "INode.h"
#include "DirectoryEntry.h"
#include "OpenFileManager.h"

class FileManager
{
public:
    /* 目录搜索模式 */
    enum DirectorySearchMode
    {
        OPEN = 0,   //以打开文件方式搜索目录
        CREATE = 1, //以新建文件方式搜索目录
        DELETE = 2
    };

public:
    FileManager();
    ~FileManager();

    void Initialize(bool flag);
    Inode* NameI(const char *pathname, enum DirectorySearchMode mode, bool& flag);//目录搜索，将路径转换成inode
    Inode* MakeNode(unsigned int mode); //被fcreat调用，用于分配内核资源
    void WriteDir(Inode* pInode);       //向父目录的目录文件写入一个目录项
    void SetCurDir(const char* pathname);     //设置当前工作路径
    void ChDir(const char *pathname);                       //改变当前工作目录

    Inode* fopen(const char *name);
    void fclose();
    int fread(Inode *pInode, char *buf, int length);
    int fwrite(Inode *pInode, char *buf,int length);
    int fcreat(const char *name,int mode);
    void fdelete(const char *name);
    void ls();

public:
    Inode* rootDirInode;    //根目录inode指针
    Inode* cdir;            //指向当前目录的inode指针
    Inode* pdir;            //指向父目录的inode指针
    DirectoryEntry dent;    //当前目录的目录项
    char dbuf[DirectoryEntry::DIRSIZE]; //当前路径分量
    char curdir[128];       //当前工作目录完整路径

    FileSystem* m_FileSystem;
    InodeTable* m_InodeTable;

    unsigned char* m_Base;  //当前读、写用户目标区域的首地址
    int m_Offset;           //当前读、写文件的字节偏移量
    int m_Count;            //当前还剩余的读、写字节数量
    int f_Offset;           //文件读写位置
};

#endif // FILEMANAGER_H

