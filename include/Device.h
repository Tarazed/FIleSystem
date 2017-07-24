#ifndef DEVICE_H
#define DEVICE_H

#include "Buf.h"
#include <iostream>
#include <fstream>

using namespace std;

class Disk
{
public:
    fstream stream;

public:
    void DevStart(Buf* bp);
    void Initialize();
    int OpenDisk();
    void Read(unsigned char* b_addr, int bwcount);
    void Write(unsigned char* b_addr, int bwcount);
};

#endif
