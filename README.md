# LFTP

## Introduction

* Use UDP as the transport layer protocol
* Realize 100% reliability as TCP
* Implement flow control function similar as TCP
* Implement congestion control function similar as TCP
* Be able to support multiple clients as the same time  

## Usage

* Server
    ```shell
    g++ server.cpp main.cpp -o LFTP -lwsock32 -std=c++11    ## 编译
    LFTP.exe                                                ## 启动服务器
    ```
* Client
    ```shell
    g++ client.cpp main.cpp -o LFTP -lwsock32 -std=c++11    ## 编译
    LFTP.exe lget server file                               ## 使用legt
    LFTP.exe lsend server file                              ## 使用lsend
    ```
    * Demo
        ```shell
        lftp.exe lget 127.0.0.1:8888 server.txt
        lftp.exe lsend 127.0.0.1:8888 ../data/client.txt
        ```