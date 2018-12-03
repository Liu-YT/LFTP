#include "client.h"

// 互斥锁
HANDLE hMutex = CreateMutex(NULL, FALSE, NULL);

Client::Client(string _ip, string _file, int _port)
{

    cout << "Initializing client...\n";
    this->serverIp = _ip;
    this->file = _file;
    this->serPort = _port;

    this->sockVersion = MAKEWORD(2, 2);
    if (WSAStartup(sockVersion, &wsaData) != 0)
        exit(1);

    this->cltSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (cltSocket == INVALID_SOCKET)
    {
        cerr << "socket create error!" << endl;
        exit(2);
    }

    this->serAddr.sin_family = AF_INET;
    ;
    this->serAddr.sin_port = htons(this->serPort);
    this->serAddr.sin_addr.S_un.S_addr = inet_addr(this->serverIp.c_str());

    this->addrLen = sizeof(serAddr);

    // 初始化拥塞窗口信息
    this->MSS = 1;
    this->ssthresh = 32;
    this->cwnd = MSS;

    cout << "Server socket created successfully..." << endl;
}

Client::~Client()
{
    closeConnect();
}

void Client::closeConnect()
{
    ::closesocket(cltSocket);
    ::WSACleanup();
    cout << "Socket closed..." << endl;
}

// lget
void Client::lsend()
{

    /*
    *   lsend
    *   file     文件名，包含具体路径 
    *   fileName 文件名，不含路径
    */
    string fileName = file.substr(file.find_last_of('/') + 1);

    // 打开文件
    ifstream readFile(file.c_str(), ios::in | ios::binary); //二进制读方式打开
    if (readFile == NULL)
    {
        cout << "File: " << file << " Can Not Open To Write" << endl;
        closeConnect();
        exit(1);
    }

    UDP_PACK pack;
    string str = "send" + fileName;
    pack.infoLength = str.size();
    for (int i = 0; i < str.size(); ++i)
        pack.info[i] = str[i];
    pack.info[str.size()] = '\0';

    // 发送文件名
    if (sendto(cltSocket, (char *)&pack, sizeof(pack), 0, (sockaddr *)&serAddr, addrLen) < 0)
    {
        cout << "Send File Name: " << file << " Failed" << endl;
        closeConnect();
        exit(1);
    }

    // 创建处理线程
    thread sendHandle(&Client::lsendOpResponse, this);

    // 定时重发
    thread timerHandle(&Client::reTransfer, this);

    while(true) {
        if (recvfrom(cltSocket, (char *)&pack, sizeof(pack), 0, (sockaddr *)&serAddr, &addrLen) > 0)
        {
            WaitForSingleObject(hMutex, INFINITE);
            win.push(pack);
            ReleaseMutex(hMutex);
        }
    }
    closeConnect();
}

// lsend
void Client::lget()
{
    /*
    *   file 文件名，不含路径 
    */
    UDP_PACK pack;
    string str = "lget" + file;
    pack.infoLength = str.size();
    for (int i = 0; i < str.size(); ++i)
        pack.info[i] = str[i];
    pack.info[str.size()] = '\0';
    pack.rwnd = RWND_MAX_SIZE - win.size();

    // 发送文件名 
    if (sendto(cltSocket, (char *)&pack, sizeof(pack), 0, (sockaddr *)&serAddr, addrLen) < 0)
    {
        cout << "Send File Name: " << file << " Failed" << endl;
        closeConnect();
        exit(1);
    }


    // 创建处理线程
    thread getHandler(&Client::lgetOpReponse, this);
    
    while (true)
    {
        if (recvfrom(cltSocket, (char *)&pack, sizeof(pack), 0, (sockaddr *)&serAddr, &addrLen) > 0)
        {
            if(win.size() < RWND_MAX_SIZE) 
            {
                WaitForSingleObject(hMutex, INFINITE);
                win.push(pack);
                ReleaseMutex(hMutex);
            }
        }
    }
    closeConnect();
}


// lget thread do
void Client::lgetOpReponse()
{

    /* 打开文件，准备写入 */
    string filePath = "../data/" + file;

    // 判断文件是否已经存在
    bool isFileExist = (_access(filePath.c_str(), 0) != -1);
    
    // cout << filePath << endl;

    ofstream writerFile(filePath.c_str(), ios::out | ios::binary);
    if (writerFile == NULL)
    {
        cout << "File: " << filePath << " Can Not Open To Write" << endl;
        closeConnect();
        exit(2);
    }

    /* 从服务器接收数据，并写入文件 */
    int ack = 0;
    int sendSeq = 0;
    while(true) {
        if (!win.empty()) 
        {
            WaitForSingleObject(hMutex, INFINITE);
            UDP_PACK pack = win.front();
            string info = string(pack.info);
            string op = info.substr(0, 4);
            if (op != "lget")
                continue;
            else
                win.pop();
            ReleaseMutex(hMutex);
            cout << "receive: ack: " << pack.ack << " seq: " << pack.seq << " " << "FIN: " << pack.FIN << endl;
            if (pack.FIN && pack.seq == -1)
            {
                // 没有相应文件
                cout << "The server no such a file - " << file << endl;
                UDP_PACK confirm = pack;
                confirm.ack = -1;
                confirm.seq = sendSeq;
                confirm.FIN = true;
                confirm.rwnd = RWND_MAX_SIZE - win.size();
                writerFile.close();
                if (!isFileExist) 
                {
                    string rm = "rm -rf " + filePath;
                    system(rm.c_str());
                }
                closeConnect();
                exit(0);
            }
            if (pack.seq == ack)
            {
                ++sendSeq;
                ++ack;

                // 写入信息
                writerFile.write((char *)&pack.data, pack.dataLength);

                // 发送确认包
                UDP_PACK confirm = pack;
                confirm.ack = pack.seq + 1;
                confirm.seq = sendSeq;
                confirm.rwnd = RWND_MAX_SIZE - win.size();
                sendto(cltSocket, (char *)&confirm, sizeof(confirm), 0, (sockaddr *)&serAddr, addrLen);
                cout << "send ack: " << confirm.ack << " seq: " << confirm.seq << " "
                     << "FIN: " << confirm.FIN << " rwnd: " << confirm.rwnd << endl;
                if (pack.FIN)
                {
                    // 结束
                    writerFile.close();
                    cout << "Transfer file " << file << " successfully " << endl;
                    closeConnect();
                    exit(0);
                }
            }
            else if (pack.seq > ack)
            {
                ++sendSeq;
                UDP_PACK confirm = pack;
                confirm.ack = ack;
                confirm.FIN = false;
                confirm.seq = sendSeq;
                confirm.rwnd = RWND_MAX_SIZE - win.size();
                // 如果接收的seq和期待的不相同,重新发送
                sendto(cltSocket, (char *)&confirm, sizeof(confirm), 0, (sockaddr *)&serAddr, addrLen);
                cout << "send ack: " << confirm.ack << " seq: " << confirm.seq << " "
                     << "FIN: " << confirm.FIN << " rwnd: " << confirm.rwnd << endl;
            }
        }       
    }
}


// lsend thread do
void Client::lsendOpResponse()
{
    /*
    *   lsend
    *   file     文件名，包含具体路径 
    *   fileName 文件名，不含路径
    */
    string fileName = file.substr(file.find_last_of('/') + 1);

    while(true) {
        if(!win.empty())
        {
            WaitForSingleObject(hMutex, INFINITE);
            UDP_PACK pack = win.front();
            string info = string(pack.info);
            string op = info.substr(0, 4);
            if(op != "send")    continue;
            else    win.pop();
            ReleaseMutex(hMutex);
            cout << "receive: ack: " << pack.ack << " seq: " << pack.seq << " " << "FIN: " << pack.FIN << " rwnd: " << pack.rwnd << endl;

            if(pack.seq == -1 && pack.FIN) {
                closeConnect();
                cout << "The file: " << fileName << " is existed in server" << endl;
                exit(0);
            }

            if (pack.FIN)
            {
                cout << "\nUpload file: " << fileName << " successfully" << endl;
                closeConnect();
                exit(0);
            }

            if (pool.size() > 0 && pool[0].seq <= pack.ack)
            {
                // 正确ack
                /* 
                * cwnd <= ssthresh 则 cwnd = cwnd * 2 
                * cwnd > ssthresh 则 cwnd = cwnd + MSS 
                */
                if (cwnd <= ssthresh)
                    cwnd *= 2;
                else
                    cwnd += 1;
            }
            else
            {
                // 错误ack
                errorNum++;
                if (errorNum == 3)
                {
                    // ssthresh = cwnd / 2
                    ssthresh = cwnd / 2;
                    // cwnd = ssthresh + 3 MMS
                    cwnd = ssthresh + 3 * MSS;
                }
            }

            // 更新滑动窗口
            int size = pool.size();
            for(int i = 0; i < pool.size(); ++i) 
            {
                if (pool[i].seq < pack.ack)
                {
                    pool.erase(pool.begin());
                    i--;
                    // 重新计时
                    timer = clock();
                }
                else
                    pool[i].ack = pack.seq + 1;
            }

            // while(pool.size() > pack.rwnd) {
            //     // 缩减窗口
            //     pool.pop_back();
            // }

            // 打开文件
            ifstream readFile(file.c_str(), ios::in | ios::binary); //二进制读方式打开

            if (pool.size() == 0)
                readFile.seekg(pack.totalByte, ios::beg);
            else
                readFile.seekg(pool[pool.size() - 1].totalByte, ios::beg);

            // 窗口前移
            for (int i = pool.size(); i < pack.rwnd && i < cwnd; ++i)
            {
                UDP_PACK newPack = pack;
                if (i >= 1)
                {
                    newPack.ack = pool[i - 1].ack;
                    newPack.seq = pool[i - 1].seq + 1;
                    newPack.dataLength = DATA_SIZE;
                    try
                    {
                        // 读取文件
                        readFile.read((char *)&newPack.data, newPack.dataLength);
                        newPack.dataLength = readFile.gcount();
                        if (readFile.peek() == EOF) {
                            newPack.FIN = true;
                        }
                        newPack.totalByte = pool[i - 1].totalByte + newPack.dataLength;
                        pool.push_back(newPack);
                        sendto(cltSocket, (char *)&newPack, sizeof(newPack), 0, (sockaddr *)&serAddr, addrLen);
                        cout << "send: ack: " << newPack.ack << " seq: " << newPack.seq << " "  << "FIN: " << newPack.FIN << " size: " << newPack.dataLength << " rwnd: " << newPack.rwnd << endl;
                        if(readFile.peek() == EOF)  break;
                    }
                    catch (exception &err)
                    {
                        newPack.FIN = true;
                        sendto(cltSocket, (char *)&newPack, sizeof(newPack), 0, (sockaddr *)&serAddr, addrLen);
                        cout << "send: ack: " << newPack.ack << " seq: " << newPack.seq << " " << "FIN: " << newPack.FIN << " size: " << newPack.dataLength << " rwnd: " << newPack.rwnd << endl;
                        cout << "Have a error" << endl;
                        readFile.close();
                        break;
                    }
                }
                else
                {
                    newPack.ack = pack.seq + 1;
                    newPack.seq = pack.ack;
                    newPack.dataLength = DATA_SIZE;
                    try
                    {
                        // 读取文件
                        readFile.read((char *)&newPack.data, newPack.dataLength);
                        newPack.dataLength = readFile.gcount();
                        if (readFile.peek() == EOF)
                        {
                            newPack.FIN = true;
                        }
                        newPack.totalByte = pack.totalByte + newPack.dataLength;
                        pool.push_back(newPack);
                        sendto(cltSocket, (char *)&newPack, sizeof(newPack), 0, (sockaddr *)&serAddr, addrLen);
                        timer = clock();
                        cout << "send: ack: " << newPack.ack << " seq: " << newPack.seq << " " << "FIN: " << newPack.FIN << " rwnd: " << newPack.rwnd << endl;
                        if(readFile.peek() == EOF)  break;
                    }
                    catch (exception &err)
                    {
                        newPack.FIN = true;
                        sendto(cltSocket, (char *)&newPack, sizeof(newPack), 0, (sockaddr *)&serAddr, addrLen);
                        cout << "send: ack: " << newPack.ack << " seq: " << newPack.seq << " " << "FIN: " << newPack.FIN << " rwnd: " << newPack.rwnd << endl;
                        cout << "Have a error" << endl;
                        readFile.close();
                        break;
                    }
                }
            }
            readFile.close();
        }
    }
}

/*
*   超时重发
*/
void Client::reTransfer()
{
    while (true)
    {
        clock_t now = clock();
        
        // 3s重传
        if (((now - timer) * 1.0 / CLOCKS_PER_SEC) >= 5.0)
        {
            // ssthresh变成之前的cwnd的一半
            ssthresh = cwnd / 2;

            // cwnd调成1
            cwnd = MSS;

            // 重发已发送未被确认的数据包
            for (int i = 0; i < pool.size(); ++i)
            {
                sendto(cltSocket, (char *)&pool[i], sizeof(pool[i]), 0, (sockaddr *)&serAddr, addrLen);
                // cout << "RE_send: ack: " << pool[i].ack << " seq: " << pool[i].seq << " "
                //      << "FIN: " << pool[i].FIN << " size: " << pool[i].dataLength << " rwnd: " << pool[i].rwnd << endl;
            }
            timer = clock();
        }
    }
}