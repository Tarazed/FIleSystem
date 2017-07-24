#ifndef FILE_SYS_H
#define FILE_SYS_H

#include "INode.h"
#include "Buf.h"
#include "BufferManager.h"

class SuperBlock//超级块定义
{
public:
	SuperBlock();
	~SuperBlock();
public:

	int s_isize;	//外存Inode区占用的盘块数
	int s_fsize;	//盘块总数

	int s_nfree;	//空闲盘块数
	int s_free[100];//空闲盘块索引

	int s_ninode;	//空闲inode数
	int s_inode[100];//空闲外存inode索引

	int s_flock;	//封锁空闲盘块索引表标志
	int s_ilock;	//封锁空闲inode表标志

	int s_fmod;		//内存中SuperBlock副本被修改标志，需要更新
	int s_ronly;	//本文件系统只能读出
	int s_time;		//最后一次更新时间
	int padding[47];//填充，使SuperBlock占满2个扇区
};

class FileSystem
{
public:
	static const int SUPER_BLOCK_SECTOR_NUMBER = 1;	//SuperBlock起始于1#扇区
	static const int ROOTINO = 0;					//系统根目录外存inode编号

	static const int INODE_NUMBER_PER_SECTOR = 8;	//一个扇区能存放8个inode
	static const int INODE_ZONE_START_SECTOR = 3;	//inode区起始于3#扇区
	static const int INODE_ZONE_SIZE = 4551;		//inode区4551个扇区

	static const int DATA_ZONE_START_SECTOR = 4554;	//数据区的起始扇区号
	static const int DATA_ZONE_END_SECTOR = 40956;	//数据区的结束扇区号
	static const int DATA_ZONE_SIZE = 40957-DATA_ZONE_START_SECTOR;	//数据区大小

public:
	FileSystem();
	~FileSystem();
	void Initialize();//初始化磁盘
    SuperBlock* GetSuperBlock();
	void LoadSuperBlock();//系统初始化时读入SuperBlock
	void Update();//更新SuperBlock
	Inode* IAlloc();//分配空闲inode
	void IFree(int number);//释放inode
	Buf* BAlloc();//分配空闲盘块
	void BFree(int blkno);//释放空闲盘块

private:
	BufferManager* bufferManager;
public:
    SuperBlock* g_spb;
};

#endif
