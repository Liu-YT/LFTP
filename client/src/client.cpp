#include "client.h"

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

    while(true) {

    }
}

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

    // 发送文件名 
    if (sendto(cltSocket, (char *)&pack, sizeof(pack), 0, (sockaddr *)&serAddr, addrLen) < 0)
    {
        cout << "Send File Name: " << file << " Failed" << endl;
        closeConnect();
        exit(1);
    }

    /* 打开文件，准备写入 */
    string filePath = "../data/" + file;
    // cout << filePath << endl;
    ofstream writerFile(filePath.c_str(), ios::out | ios::binary);
    if (NULL == writerFile)
    {
        cout << "File: " << filePath << " Can Not Open To Write" << endl;
        closeConnect();
        exit(2);
    }

    /* 从服务器接收数据，并写入文件 */
    int ack = 0;
    int sendSeq = 0;
    while (true)
    {
        if (recvfrom(cltSocket, (char *)&pack, sizeof(pack), 0, (sockaddr *)&serAddr, &addrLen) > 0)
        {
            cout << "ack: " << pack.ack << " seq: " << pack.seq << " " << "FIN: " << pack.FIN << " " << pack.dataLength << endl;
            if (pack.FIN && pack.seq == -1)
            {
                // 没有相应文件
                cout << "No such a file - " << file << endl;
                UDP_PACK confirm = pack;
                confirm.ack = pack.seq + 1;
                confirm.seq = sendSeq;
                confirm.FIN = true;
                sendto(cltSocket, (char *)&confirm, sizeof(confirm), 0, (sockaddr *)&serAddr, addrLen);
                writerFile.close();
                string rm = "rm -rf " + filePath;
                system(rm.c_str());
                closeConnect();
                exit(0);
            }
            if(pack.seq == ack) {
                ++sendSeq;
                ++ack;
                
                // 写入信息
                writerFile.write((char *)&pack.data, pack.dataLength);


                // 发送确认包
                UDP_PACK confirm = pack;
                confirm.ack = pack.seq + 1;
                confirm.seq = sendSeq;
                sendto(cltSocket, (char *)&confirm, sizeof(confirm), 0, (sockaddr *)&serAddr, addrLen);
                if (pack.FIN)
                {
                    // 结束
                    writerFile.close();
                    cout << "Transfer file " << file << " successfully " << endl;
                    closeConnect();
                    exit(0);
                }
                cout << "send: ack " << confirm.ack << endl;
            }
            else 
            {
                ++sendSeq;
                UDP_PACK confirm = pack;
                confirm.ack = ack;
                confirm.seq = sendSeq;
                // 如果接收的seq和期待的不相同,重新发送
                sendto(cltSocket, (char *)&confirm, sizeof(confirm), 0, (sockaddr *)&serAddr, addrLen);
                cout << "send: ack " << confirm.ack << endl;
            }
        }
    }
    closeConnect();
}