#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <winsock2.h>
#include <windows.h>
#include <string>
#include <iostream>
#include <fstream>
#include <ctime>
#include <thread>
#include <vector>
#include <queue>
#include "package.h"

using namespace std;
using std::thread;

// 流量控制
#define RWND_MAX_SIZE 50

// 每个数据包的存储的数据长度
#define DATA_SIZE 2048

#pragma comment(lib, "ws2_32.lib")

class Client
{

  public:
    Client(string _ip = "127.0.0.1", string _file = "../data/", int _port = 8888);
    ~Client();
    void closeConnect();
    void lsend();
    void lget();
    void lgetOpReponse();
    void lsendOpResponse();
    void reTransfer();
    queue<UDP_PACK> win;   // 接收窗口（流量控制）
    vector<UDP_PACK> pool;  // 发送窗口
    clock_t timer;          // 定时器


  private:
    string file;      // 文件
    int serPort;      // 服务器端口
    string serverIp;  // 服务器 ip
    WSADATA wsaData;  
    WORD sockVersion;
    SOCKET cltSocket;
    SOCKADDR_IN serAddr;
    int addrLen;
    int cwnd;       // 拥塞窗口
    int rwnd;       // 接收窗口大小（流量控制）
    int MSS;
    int ssthresh;
};

#endif