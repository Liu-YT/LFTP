# LFTP

## 介绍


## 运行命令

* 服务端
    ```shell
    g++ server.cpp main.cpp -o lftp -lwsock32 -std=c++11    ## 编译
    lftp.exe                                                ## 启动服务器
    ```
* 客户端
    * 说明
        ```shell
        g++ client.cpp main.cpp -o lftp -lwsock32   ## 编译
        lftp.exe lget server filename               ## 使用legt
        lftp.exe lsend server filename              ## 使用lsend
        ```
    * 例子
        ```shell
        lftp.exe lget 127.0.0.1:8888 server.txt
        lftp.exe lsend 127.0.0.1:8888 ../data/client.txt
        ```

## 设计

* RDT
    * Go-Back-N
    * 超时重传
* 服务器同时支持多用户
    * 服务器将存在两个线程，主线程接收数据包，并将收到的包放在一个池中，另一个线程对池中的包进行处理
* 流量控制
    * 
* 拥塞控制