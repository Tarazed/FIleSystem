#ifndef OPEN_FILE_MANAGER_H
#define OPEN_FILE_MANAGER_H

#include "INode.h"
#include "fileSys.h"


class InodeTable
{
public:
    static const int NINODE = 100;//内存inode数量

public:
    InodeTable();
    ~InodeTable();

    void Initialize();
    Inode* IGet(int inumber);   //获取外存inode
    void IPut(Inode* pNode);    /*减少该内存Inode的引用计数，如果此Inode已经没有目录项指向它，
                                 * 且无进程引用该Inode，则释放此文件占用的磁盘块。*/
    void UpdateInodeTable();    //将所有被修改过的内存Inode更新到对应外存Inode中
    int  IsLoaded(int inumber); //检查inode是否在内存中有拷贝
    Inode* GetFreeInode();      //获得内存inode表中一个空闲inode

public:
    Inode m_Inode[NINODE];
    FileSystem* fileSys;
};

#endif // OPENFILEMANAGER_H

