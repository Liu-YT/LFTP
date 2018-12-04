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

## Note
* **Each client can only support one operation(lsend or lget) at a time.**
* The default port used by the server is `8888`, the port can be modified by the default port of the `Server` constructor
    * ` Server(string _dir = "../data/", int port = 8888)`
* The default receive transfer file is stored in the `../data/` directory，both of the server and client
* When using `lsend`, you need to give the file path relative to `src`, use relative path only for `/`
* Please input the command parameters as required, otherwise unnecessary errors will occur

## Design
* [Design Document](./docs/Project_design.md)

## Test
* [Test Document](./docs/Project_test.md)