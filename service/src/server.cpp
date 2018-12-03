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
    this->MSS = 1;

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
    // thread packHandler(&Server::deal, this);
    // packHandler.detach();
    thread timeHandler(&Server::reTransfer, this);
    timeHandler.detach();
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
                    ipToAddr[cltAddr.sin_addr.S_un.S_addr] = cltAddr;
                    if(pack.ack == 0)
                        deal(rec, cltAddr.sin_addr.S_un.S_addr);
                    else
                        pool[cltAddr.sin_addr.S_un.S_addr].push(rec);
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

/*
*   创建线程处理数据包
*/
void Server::deal(UDP_PACK pack, u_long ip)
{
    // 第一次连接
    if (pack.ack == 0)
    {
        cout << "Connected with: " << ip << endl;
        /* 获取必要信息 */
        string info = string(pack.info);
        string op = info.substr(0, 4);
        string fileName = info.substr(4, pack.infoLength - 4);
        if (op == "send")
        {
            // 检查文件是否存在
            SOCKADDR_IN addr = ipToAddr[ip];
            string filePath = dataDir + fileName;
            if (_access(filePath.c_str(), 0) != -1 && pack.ack == 0)
            {
                // 文件已经存在
                UDP_PACK confirm = pack;
                confirm.ack = pack.seq + 1;
                confirm.seq = -1;
                confirm.FIN = true;
                sendto(serSocket, (char *)&confirm, sizeof(confirm), 0, (sockaddr *)&addr, addrLen);
                cout << "Close connection with: " << ip << endl;
            }
            else if (pack.ack == 0)
            {
                waitAck[addr.sin_addr.S_un.S_addr] = 1;

                UDP_PACK confirm = pack;
                confirm.ack = pack.seq + 1;
                confirm.seq = 0;
                confirm.rwnd = RWND_MAX_SIZE;
                confirm.FIN = false;

                // 防止之前的延迟包
                map<u_long, queue<UDP_PACK>>::iterator a = pool.find(addr.sin_addr.S_un.S_addr);
                if (a != pool.end())    pool.erase(a);

                sendto(serSocket, (char *)&confirm, sizeof(confirm), 0, (sockaddr *)&addr, addrLen);
                try
                {
                    thread(&Server::lSend, this, ip, filePath).detach();
                }
                catch (exception &err)
                {
                    cout << err.what() << endl;
                }
            }
        }
        else if (op == "lget")
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
                cout << "Close connection with: " << ip << endl;

            }
            else
            {

                cwnd[ip] = MSS;
                ssthresh[ip] = 32;
                errorNum[ip] = 0;

                UDP_PACK confirm = pack;
                confirm.ack = pack.seq + 1;
                confirm.seq = 0;
                confirm.FIN = false;
                sendto(serSocket, (char *)&confirm, sizeof(confirm), 0, (sockaddr *)&cltAddr, addrLen);
                try
                {
                    thread(&Server::lGet, this, ip, filePath).detach();
                }
                catch(exception &err){
                    cout << err.what() << endl;
                }
            }
            readFile.close();
        }
    }
}

/*
*   Todo: 发送客户端请求的文件
*   
*   param
*       ip - 能够对应到客户端的地址信息
*       filePath - 文件路径
*/
void Server::lGet(u_long ip, string filePath)
{

    queue<UDP_PACK> &recWin = pool[ip];
    queue<UDP_PACK> &buffer = bufferWin[ip];
    SOCKADDR_IN addr = ipToAddr[ip];

    while(true) {
        if(!recWin.empty()) 
        {
            WaitForSingleObject(hMutex, INFINITE);
            UDP_PACK pack = recWin.front();
            string info = string(pack.info);
            string op = info.substr(0, 4);
            if (op != "lget")
                continue;
            else
                recWin.pop();
            ReleaseMutex(hMutex);

            /* 处理确认的信息 */
            if (pack.FIN)
            {
                /* 结束 */

                // 清除相应配置信息
                WaitForSingleObject(hMutex, INFINITE);
                map<u_long, queue<UDP_PACK>>::iterator a = pool.find(addr.sin_addr.S_un.S_addr);
                if (a != pool.end())
                    pool.erase(a);

                map<u_long, queue<UDP_PACK>>::iterator b = bufferWin.find(addr.sin_addr.S_un.S_addr);
                if (b != bufferWin.end())
                    bufferWin.erase(b);

                map<u_long, SOCKADDR_IN>::iterator c = ipToAddr.find(addr.sin_addr.S_un.S_addr);
                if(c != ipToAddr.end())
                    ipToAddr.erase(c);

                map<u_long, clock_t>::iterator d = timer.find(addr.sin_addr.S_un.S_addr);
                if(d != timer.end())
                    timer.erase(d);
                ReleaseMutex(hMutex);

                waitAck[addr.sin_addr.S_un.S_addr] = 0;

                cout << "Close connection with: " << ip << endl;

                return;
            }

            // 调整拥塞窗口

            // 正确ack
            if(buffer.front().seq <= pack.ack)
            {
                /* 
                * cwnd <= ssthresh 则 cwnd = cwnd * 2 
                * cwnd > ssthresh 则 cwnd = cwnd + MSS 
                */
                if(cwnd[ip] <= ssthresh[ip])
                    cwnd[ip] *= 2;
                else
                    cwnd[ip] += 1; 
               
            }
            else
            {
                errorNum[ip]++;
                if(errorNum[ip] == 3)
                {
                    // ssthresh = cwnd / 2
                    ssthresh[ip] = cwnd[ip] / 2;
                    // cwnd = ssthresh + 3 MMS
                    cwnd[ip] = ssthresh[ip] + 3 * MSS;
                }
            }

            // 滑动窗口
            for (int i = 0; i < buffer.size(); ++i)
            {
                if (buffer.front().seq < pack.ack)
                {
                    buffer.pop();
                    --i;
                    // 更新超时
                    timer[addr.sin_addr.S_un.S_addr] = clock();
                }
                else
                    buffer.front().ack = pack.seq + 1;
            }

            // 发送方中未被确认的数据量不会超过 cwnd 与 rwnd 中的 最小值
            // while (buffer.size() > pack.rwnd)
            // {
            //     // 缩减窗口
            //     buffer.pop_back();
            // }

            // 二进制读方式打开
            ifstream readFile(filePath.c_str(), ios::in | ios::binary);
            if (buffer.size() == 0)
                readFile.seekg(pack.totalByte, ios::beg);
            else
                readFile.seekg(buffer.back().totalByte, ios::beg);

            if(buffer.size() > 0 && buffer.back().FIN)   continue;;

            // 窗口前移
            for (int i = buffer.size(); i < pack.rwnd && i < cwnd[ip]; ++i)
            {
                UDP_PACK newPack = pack;
                if (i >= 1)
                {
                    newPack.ack = buffer.front().ack;
                    newPack.seq = buffer.back().seq + 1;
                    newPack.dataLength = DATA_SIZE;
                    newPack.totalByte = buffer.back().totalByte + newPack.dataLength;
                    try
                    {
                        // 读取文件
                        readFile.read((char *)&newPack.data, newPack.dataLength);
                        newPack.dataLength = readFile.gcount();
                        if (readFile.peek() == EOF)
                            newPack.FIN = true;
                        buffer.push(newPack);
                        sendto(serSocket, (char *)&newPack, sizeof(newPack), 0, (sockaddr *)&addr, addrLen);
                        cerr << "lGet: ack: " << newPack.ack << " seq: " << newPack.seq << " fin: " << newPack.FIN << endl;

                        if(readFile.peek() == EOF)  break;
                    }
                    catch (exception &err)
                    {
                        readFile.close();
                        break;
                    }
                }
                else
                {
                    newPack.ack = pack.seq + 1;
                    newPack.seq = pack.ack;
                    newPack.dataLength = DATA_SIZE;
                    newPack.totalByte = pack.totalByte + newPack.dataLength;
                    try
                    {
                        // 读取文件
                        readFile.read((char *)&newPack.data, newPack.dataLength);
                        newPack.dataLength = readFile.gcount();
                        if (readFile.peek() == EOF)
                            newPack.FIN = true;
                        buffer.push(newPack);
                        sendto(serSocket, (char *)&newPack, sizeof(newPack), 0, (sockaddr *)&addr, addrLen);
                        cerr << "lGet win 0: ack: " << newPack.ack << " seq: " << newPack.seq << " fin: " << newPack.FIN << endl;
                        timer[addr.sin_addr.S_un.S_addr] = clock();
                        if(readFile.peek() == EOF)  break;
                    }
                    catch (exception &err)
                    {
                        readFile.close();
                        break;
                    }
                }
                if (readFile.peek() == EOF)
                    break;
            }
            readFile.close();
        }
    }
}

/*
*   Todo: 接收客户端上传的文件.
*   
*   param
*       ip - 能够对应到客户端的地址信息
*       filePath - 文件路径
*/
void Server::lSend(u_long ip, string filePath)
{
    queue<UDP_PACK>& packs = pool[ip];
    SOCKADDR_IN addr = ipToAddr[ip];
    waitAck[addr.sin_addr.S_un.S_addr] = 1;
    int &ack = waitAck[addr.sin_addr.S_un.S_addr];
    // seq
    int sendSeq = 1;

    while(true) 
    {
        if(!packs.empty()) {
            while(!packs.empty()) {

                WaitForSingleObject(hMutex, INFINITE);
                UDP_PACK pack = packs.front();
                string info = string(pack.info);
                string op = info.substr(0, 4);
                if(op != "send")
                    continue;
                else
                    packs.pop();
                ReleaseMutex(hMutex);


                if (pack.seq == ack)
                {
                    ack++;
                    // 写入信息
                    ofstream writerFile(filePath.c_str(), ios::app | ios::binary);
                    writerFile.write((char *)&pack.data, pack.dataLength);
                    writerFile.close();

                    if (packs.size() > 0 && packs.front().seq != ack || packs.empty())
                    {
                        // 发送确认包
                        UDP_PACK confirm = pack;
                        confirm.ack = ack;
                        confirm.seq = sendSeq++;
                        confirm.rwnd = ((RWND_MAX_SIZE - packs.size() <= 0) ? 1 : RWND_MAX_SIZE - packs.size());
                        sendto(serSocket, (char *)&confirm, sizeof(confirm), 0, (sockaddr *)&addr, addrLen);
                        cerr << "lsend: ack: " << confirm.ack << " seq: " << confirm.seq << " fin: " << confirm.FIN << endl;

                        if (pack.FIN)
                        {
                            /* 结束 */

                            // 清除相应配置信息
                            map<u_long, queue<UDP_PACK>>::iterator a = pool.find(addr.sin_addr.S_un.S_addr);
                            if (a != pool.end())
                                pool.erase(a);

                            // map<u_long, queue<UDP_PACK>>::iterator b = bufferWin.find(addr.sin_addr.S_un.S_addr);
                            // if (b != bufferWin.end())
                            //     bufferWin.erase(b);

                            map<u_long, SOCKADDR_IN>::iterator c = ipToAddr.find(addr.sin_addr.S_un.S_addr);
                            if (c != ipToAddr.end())
                                ipToAddr.erase(c);

                            waitAck[addr.sin_addr.S_un.S_addr] = 0;

                            cout << "Close connection with: " << ip << endl;

                            return;
                        }
                    }
                }
                else
                {
                    // 接收到重复的包处理
                    UDP_PACK confirm = pack;
                    confirm.ack = ack;
                    confirm.FIN = false;
                    confirm.seq = sendSeq++;
                    confirm.rwnd = ((RWND_MAX_SIZE - packs.size() <= 0) ? 1 : RWND_MAX_SIZE - packs.size());
                    // 如果接收的seq和期待的不相同,重新发送
                    sendto(serSocket, (char *)&confirm, sizeof(confirm), 0, (sockaddr *)&addr, addrLen);
                }
            }
            
        }
    }
}

/*
*   超时重发，调整拥塞窗口大小以及阈值
*/
void Server::reTransfer()
{
    while (true)
    {
        map<u_long, clock_t>::iterator it = timer.begin();
        while (it != timer.end())
        {
            u_long ipAddr = it->first;
            clock_t start = it->second;
            clock_t now = clock();
            // 3s重传
            if (((now - start) * 1.0 / CLOCKS_PER_SEC) >= 3.0)
            {
                // ssthresh变成之前的cwnd的一半
                ssthresh[ipAddr] = cwnd[ipAddr] / 2;

                // cwnd调成1
                cwnd[ipAddr] = MSS; 

                // 获取已发送未被确认的数据包
                queue<UDP_PACK> &buffer = bufferWin[ipAddr];
                SOCKADDR_IN cltAddr = ipToAddr[ipAddr];
                // 重传
                sendto(serSocket, (char *)&buffer.front(), sizeof(buffer.front()), 0, (sockaddr *)&cltAddr, addrLen);
                cerr << "ReSend: ack: " << buffer.front().ack << " seq: " << buffer.front().seq << " fin: " << buffer.front().FIN << endl;
                it->second = clock();
            }
            it++;
        }
    }
}