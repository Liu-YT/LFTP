#ifndef SERVER_HPP
#define SERVER_HPP

#include <winsock2.h>
#include <windows.h>
#include <string>
#include <exception>
#include <iostream>
#include <WS2tcpip.h>
#include <fstream>
#include <ctime>
#include <vector>
#include <queue>
#include <map>
#include <cstring>
#include <thread>
#include "package.h"

using namespace std;
using std::thread;

#pragma comment(lib, "ws2_32.lib")

class Server
{

  public:
    Server(string _dir = "../data/", int port = 8888);

    ~Server();

    // 关闭连接
    void closeConnect();

    void waitForClient();

    void dealGet(string file, UDP_PACK pack, SOCKADDR_IN addr);

    void dealSend(string file, UDP_PACK pack, SOCKADDR_IN addr);

    void deal();

    void reTransfer();

    // 收集得收到的数据包
    queue<UDP_PACK> recPacks;

    // 包对应的cltAddr
    queue<SOCKADDR_IN> address;

    // ip对应的SOCKADDR_IN
    map<u_long, SOCKADDR_IN> ipToAddr;

    // 维护各个连接的滑动窗口(发送文件)
    map<u_long, vector<UDP_PACK> > pool;

    // 计时器，超时重传
    map <u_long, clock_t> timer;

private : 
    string dataDir; // 文件地址
    int serPort;    // 服务端口
    WSADATA wsaData;
    WORD sockVersion;
    SOCKET serSocket;
    SOCKADDR_IN cltAddr;
    SOCKADDR_IN serAddr;
    int addrLen;
    int connectNum;
    int MSS;      // 拥塞窗口大小
    int ssthresh; // 慢启动阈值
    int cwnd;     // 拥塞窗口
    int rwnd;     // 接收窗口（流量控制）
};

#endif