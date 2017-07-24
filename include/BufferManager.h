#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include "Buf.h"

class BufferManager
{
public:
	static const int NBUF = 15; //缓存控制块、缓冲区的数量
	static const int BUFFER_SIZE = 512;	//缓冲区大小。 以字节为单位
	static const int NSECTOR = 40956;	//磁盘扇区号上限

public:
	BufferManager();
	~BufferManager();
	void Initialize();

	Buf* GetBlk(int blkno);	//申请一块缓存，用于读写设备dev上的字符块blkno
	void Brelse(Buf* bp);	//释放缓存控制块buf
	Buf* Bread(int blkno);	//读一个磁盘块。blkno为目标磁盘块逻辑块号
	void Bwrite(Buf* bp);	//写一个磁盘块

	void CLrBuf(Buf *bp);	//清空缓冲区内容
	void Bflush();			//将dev指定设备队列中延迟写的缓存全部输出到磁盘
    int  Strategy(Buf *bp); //IO

    void Start();           //启动IO

private:
	Buf bFreeList;			//自由缓存队列控制块
	Buf m_Buf[NBUF];		//缓存控制块数组
	unsigned char Buffer[NBUF][BUFFER_SIZE];	//缓冲区数组 
	/* I/O请求队列 */
	/* I/O请求队列 */
    Buf* d_actf;
    Buf* d_actl;
};

#endif
