#ifndef MY_FS_MANAGER_H
#define MY_FS_MANAGER_H

#include "BufferManager.h"
#include "Device.h"
#include "fileSys.h"
#include "OpenFileManager.h"
#include "FileManager.h"

class MyFsManager
{
public:
    MyFsManager();
    ~MyFsManager();
    static MyFsManager& Instance();
    int Initialize(bool flag);

    BufferManager& GetBufferManager();
    Disk& GetDisk();
    FileSystem& GetFileSystem();
    InodeTable& GetInodeTable();
    FileManager& GetFileManager();

private:
    static MyFsManager instance;    //单体类实例
    BufferManager* bm;
    Disk* disk;
    FileSystem* filesystem;
    InodeTable* inodetable;
    FileManager* filemanager;
};

#endif
