#ifndef DIRECTORY_ENTRY_H
#define DIRECTORY_ENTRY_H

class DirectoryEntry
{
public:
    static const int DIRSIZE = 28;  //目录项中路径部分的最大字符串长度

public:
    int m_inumber;  //目录项中inode编号
    char m_name[DIRSIZE];   //目录项中路径名部分
};

#endif

