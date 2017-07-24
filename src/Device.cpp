#include "../include/Device.h"
#include "../include/Buf.h"
#include <unistd.h>
#include <iostream>

using namespace std;

void Disk::Initialize()
{
    this->stream.open("mydisk.img",ios::in|ios::out|ios::binary|ios::trunc);
    if(!this->stream)
    {
        printf("stream err\n");
        return;
    }
    truncate("mydisk.img", 20*1024*1024);
}

int Disk::OpenDisk()
{
    this->stream.open("mydisk.img",ios::in|ios::out|ios::binary);
    if(!this->stream)
    {
        printf("Disk does not exist!The system will automally create and initialize it.\n\n\n");
        return 0;
    }
    return 1;
}

void Disk::Read(unsigned char* b_addr, int b_wcount)
{
    //fread(b_addr,b_wcount,1,this->stream);
    this->stream.read((char *)b_addr,b_wcount);
}

void Disk::Write(unsigned char* b_addr, int b_wcount)
{
    //fwrite(b_addr,b_wcount,1,this->stream);
    this->stream.write((char *)b_addr,b_wcount);
}

void Disk::DevStart(Buf *bp)
{
    if(bp == NULL)
    {
        printf("NULL BLK\n");
    }

    if((bp->b_flags & Buf::B_READ) != 0)
    {
        this->stream.seekg(bp->b_blkno*512,ios::beg);
        Read(bp->b_addr,bp->b_wcount);
    }
    else if(((bp->b_flags & Buf::B_WRITE) != 0)||((bp->b_flags & Buf::B_DELWRI) != 0))
    {
        this->stream.seekp(bp->b_blkno*512,ios::beg);
        Write(bp->b_addr,bp->b_wcount);
    }
    return ;
}
