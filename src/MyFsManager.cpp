#include "../include/MyFsManager.h"

/* 全局变量声明 */
MyFsManager MyFsManager::instance;


MyFsManager::MyFsManager()
{

}
MyFsManager::~MyFsManager()
{

}

MyFsManager& MyFsManager::Instance()
{
    return MyFsManager::instance;
}

int MyFsManager::Initialize(bool flag)
{
    int res;

    this->bm = new BufferManager();
    this->disk = new Disk();
    this->filesystem = new FileSystem();
    this->inodetable = new InodeTable();
    this->filemanager = new FileManager();

    this->bm->Initialize();
    if(flag==true)
    {
        this->disk->Initialize();
        res=1;
    }
    else
    {
        res = this->disk->OpenDisk();
    }
    this->filesystem->Initialize();
    this->inodetable->Initialize();
    if(flag==false)
        this->filemanager->Initialize(flag);

    return res;
}

BufferManager& MyFsManager::GetBufferManager()
{
    return *(this->bm);
}

Disk& MyFsManager::GetDisk()
{
    return *(this->disk);
}

FileSystem& MyFsManager::GetFileSystem()
{
    return *(this->filesystem);
}

InodeTable& MyFsManager::GetInodeTable()
{
    return *(this->inodetable);
}

FileManager& MyFsManager::GetFileManager()
{
    return *(this->filemanager);
}
