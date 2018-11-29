#include "server.h"

// 互斥锁
HANDLE hMutex = CreateMutex(NULL, FALSE, NULL);

Server::Server(string _dir, int _serPort)
{

    this->dataDir = _dir;
    this->serPort = _serPort;

    cout << "Initializing server...\n";

    //初始化socket资源
    this->sockVersion = MAKEWORD(2, 2);
    if (WSAStartup(sockVersion, &wsaData) != 0)
    {
        exit(1);
    }
    this->addrLen = sizeof(SOCKADDR_IN);

    this->serSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serSocket == INVALID_SOCKET)
    {
        cerr << "socket create error!" << endl;
        exit(2);
    }

    //设置传输协议、端口以及目的地址
    this->serAddr.sin_family = AF_INET;
    this->serAddr.sin_port = htons(this->serPort);
    this->serAddr.sin_addr.S_un.S_addr = INADDR_ANY;
    if (bind(serSocket, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR)
    {
        cerr << "socket bind error!" << endl;
        closesocket(serSocket);
        exit(3);
    }

    // 初始化拥塞窗口信息
    this->MSS = 1024;
    this->ssthresh = 4096;
    this->cwnd = MSS;

    cout << "Server socket created successfully..." << endl;
}

Server::~Server()
{
    closeConnect();
}

void Server::closeConnect()
{
    ::closesocket(serSocket);
    ::WSACleanup();
    cout << "Socket closed..." << endl;
}

void Server::waitForClient()
{
    thread handler(&Server::CreateClientThread, this);
    // 接收数据
    UDP_PACK pack;
    try
    {

        while (true)
        {
            if (recvfrom(serSocket, (char *)&pack, sizeof(pack), 0, (sockaddr *)&cltAddr, &addrLen) < 0)
            {
                cout << "Have a error!" << endl;
                closeConnect();
                exit(4);
            }
            else
            {
                try
                {
                    // 存储数据包
                    UDP_PACK rec = pack;
                    WaitForSingleObject(hMutex, INFINITE);
                    recPacks.push(rec);
                    address.push(cltAddr);
                    ReleaseMutex(hMutex);
                }
                catch (exception &err)
                {
                    cerr << err.what() << endl;
                }
            }
        }
    }
    catch (exception &err)
    {
        cerr << err.what() << endl;
    }
}

// Todo: 接收客户端发来的文件
void Server::dealSend(string fileName, UDP_PACK pack, SOCKADDR_IN addr)
{
    string filePath = dataDir + fileName;
}

// Todo: 发送客户端请求的文件
void Server::dealGet(string fileName, UDP_PACK pack, SOCKADDR_IN addr)
{
    /* 打开文件 */
    string filePath = dataDir + fileName;
    // 二进制读方式打开
    ifstream readFile(filePath.c_str(), ios::in | ios::binary);

    // 没有相应文件
    if (readFile == NULL)
    {
        UDP_PACK confirm = pack;
        confirm.ack = pack.seq + 1;
        confirm.seq = -1;
        confirm.FIN = true;
        sendto(serSocket, (char *)&confirm, sizeof(confirm), 0, (sockaddr *)&cltAddr, addrLen);
        return;
    }

    /* 处理确认的信息 */
    if (pack.FIN)
    {
        map<u_long, vector<UDP_PACK> >::iterator iter = pool.find(addr.sin_addr.S_un.S_addr);
        pool.erase(iter);
        return;
    }

    // 滑动窗口
    vector<UDP_PACK> &win = pool[addr.sin_addr.S_un.S_addr];
    while (!win.empty())
    {
        UDP_PACK isAckPack = win[0];
        if (isAckPack.seq < pack.ack)
            win.erase(win.begin());
        else
            break;
    }

    // 更新存储的数据包 ack
    for (int i = 0; i < win.size(); ++i)
    {
        if (win[i].ack == pack.seq)
            win[i].ack = pack.seq + 1;
        // sendto(serSocket, (char *)&win[i], sizeof(win[i]), 0, (sockaddr *)&addr, addrLen);
    }

    if (win.size() == 0)
        readFile.seekg(pack.totalByte, ios::beg);
    else
        readFile.seekg(win[win.size() - 1].totalByte, ios::beg);
    // 窗口前移
    for (int i = win.size(); i < winSize; ++i)
    {
        UDP_PACK newPack = pack;
        if (readFile.peek() == EOF)
            return;
        if (i >= 1)
        {
            newPack.ack = win[i - 1].ack;
            newPack.seq = win[i - 1].seq + 1;
            newPack.dataLength = cwnd;
            newPack.totalByte = win[i - 1].totalByte + newPack.dataLength;
            try
            {
                // 读取文件
                readFile.read((char *)&newPack.data, newPack.dataLength);
                newPack.dataLength = readFile.gcount();
                if (readFile.peek() == EOF)
                    newPack.FIN = true;
                win.push_back(newPack);
                sendto(serSocket, (char *)&newPack, sizeof(newPack), 0, (sockaddr *)&addr, addrLen);
            }
            catch (exception &err)
            {
                newPack.FIN = true;
                sendto(serSocket, (char *)&newPack, sizeof(newPack), 0, (sockaddr *)&addr, addrLen);
                readFile.close();
                break;
            }
        }
        else
        {
            newPack.ack = pack.seq + 1;
            newPack.seq = pack.ack;
            newPack.dataLength = cwnd;
            newPack.totalByte = pack.totalByte + newPack.dataLength;
            try
            {
                // 读取文件
                readFile.read((char *)&newPack.data, newPack.dataLength);
                if (readFile.peek() == EOF)
                {
                    newPack.FIN = true;
                    newPack.dataLength = readFile.gcount();
                    readFile.close();
                }
                win.push_back(newPack);
                sendto(serSocket, (char *)&newPack, sizeof(newPack), 0, (sockaddr *)&addr, addrLen);
            }
            catch (exception &err)
            {
                newPack.FIN = true;
                sendto(serSocket, (char *)&newPack, sizeof(newPack), 0, (sockaddr *)&addr, addrLen);
                newPack.dataLength = readFile.gcount();
                readFile.close();
                break;
            }
        }
        Sleep(1);
    }
    readFile.close();
}

void Server::CreateClientThread()
{
    while(true)
    {
        // 处理数据包
        if (!recPacks.empty())
        {
            WaitForSingleObject(hMutex, INFINITE);
            UDP_PACK proPack = recPacks.front();
            recPacks.pop();

            SOCKADDR_IN addr = address.front();
            address.pop();
            ReleaseMutex(hMutex);

            /* 获取必要信息 */
            string info = string(proPack.info);
            string op = info.substr(0, 4);
            string fileName = info.substr(4, proPack.infoLength - 4);

            if (op == "send")
            {
                // cout << "lsend" << endl;
                dealSend(fileName, proPack, addr);
            }
            else if (op == "lget")
            {
                // cout << "lget" << endl;
                dealGet(fileName, proPack, addr);
            }
            else
            {
                continue;
            }
        }
    }
}