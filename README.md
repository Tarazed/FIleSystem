## FileSystem

本系统基于ubuntu 16.04 LTS环境开发，编译器使用g++，需要用到的非标准库为unistd.h

目录说明：bin为可执行文件目录，include为头文件所在目录，obj为编译生成的.o文件所在目录，src为源程序所在目录

使用：进入makefile所在目录，执行命令make，生成可执行文件./bin/MyFs

进入bin目录，./MyFs启动程序。

注：目前删除巨型文件会报错
