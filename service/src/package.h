#ifndef PACKAGE_H
#define PACKAGE_H

#include <string>
using std::string;

#define DATA_MAX_SIZE 2048
#define INFO_MAX_SIZE 100

struct UDP_PACK
{
    int ack;                  // 确认号
    int seq;                  // 序列号
    bool FIN;                 // 结束标志
    int rwnd;                 // 窗口大小（流量控制）
    char data[DATA_MAX_SIZE]; // 传输数据
    int dataLength;           // 数据长度
    char info[INFO_MAX_SIZE]; // 操作 + 文件名
    int infoLength;           // 信息长度
    int totalByte;            // 记录发送总字节

    UDP_PACK()
    {
        ack = 0;
        seq = 0;
        dataLength = 0;
        FIN = false;
        totalByte = 0;
    }

    UDP_PACK &operator=(const UDP_PACK &pack)
    {
        ack = pack.ack;
        seq = pack.seq;
        FIN = pack.FIN;
        rwnd = pack.rwnd;
        dataLength = pack.dataLength;
        infoLength = pack.infoLength;
        totalByte = pack.totalByte;
        info[infoLength] = '\0';

        for (int i = 0; i < infoLength; ++i)
            info[i] = pack.info[i];

        for (int i = 0; i < dataLength; ++i)
            data[i] = pack.data[i];
    }
};

#endif