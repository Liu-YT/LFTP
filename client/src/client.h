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

#define RWND_MAX_SIZE 20

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
    queue<UDP_PACK> win; // 接收窗口（流量控制）

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