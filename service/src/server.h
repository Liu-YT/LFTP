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
#include <io.h>
#include <cstring>
#include <thread>
#include "package.h"

using namespace std;
using std::thread;

// 流量控制
#define RWND_MAX_SIZE 100

// 每个数据包的存储的数据长度
#define DATA_SIZE 2048

#pragma comment(lib, "ws2_32.lib")

class Server
{

  public:
    Server(string _dir = "../data/", int port = 8888);

    ~Server();

    // 关闭连接
    void closeConnect();

    void waitForClient();

    void deal(UDP_PACK pack, u_long ip);

    void reTransfer();

    // ip对应的SOCKADDR_IN
    map<u_long, SOCKADDR_IN> ipToAddr;

    // 维护各个连接的接收窗口，流量控制
    map<u_long, queue<UDP_PACK> > pool;

    // 缓存窗口，发送文件
    map<u_long, queue<UDP_PACK> > bufferWin;

    // 计时器，超时重传
    map <u_long, clock_t> timer;

    // 记录发送文件连接确认号ack
    map<u_long, int> waitAck;

    void lSend(u_long ip, string filePath);

    void lGet(u_long ip, string filePath);

  private : string dataDir; // 文件地址
    int serPort;    // 服务端口
    WSADATA wsaData;
    WORD sockVersion;
    SOCKET serSocket;
    SOCKADDR_IN cltAddr;
    SOCKADDR_IN serAddr;
    int addrLen;
    int MSS;      // 拥塞窗口大小
    int ssthresh; // 慢启动阈值
    int cwnd;     // 拥塞窗口
    // int connectNum;
};

#endif