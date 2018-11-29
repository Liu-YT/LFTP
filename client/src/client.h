#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <winsock2.h>
#include <windows.h>
#include <string>
#include <iostream>
#include <fstream>
#include <ctime>
#include "package.h"

using namespace std;

#define BUFFER_SIZE 2048

#pragma comment(lib, "ws2_32.lib")

// 滑动窗口大小
#define winSIZE 5

class Client
{

  public:
    Client(string _ip = "127.0.0.1", string _file = "../data/", int _port = 8888);
    ~Client();
    void closeConnect();
    void lsend();
    void lget();
    UDP_PACK win[winSIZE];  // 滑动窗口

  private:
    string file;      // 文件
    int serPort;      // 服务器端口
    string serverIp;  // 服务器 ip
    WSADATA wsaData;  
    WORD sockVersion;
    SOCKET cltSocket;
    SOCKADDR_IN serAddr;
    int addrLen;
    int rwnd;       // 接收窗口（流量控制）
};

#endif