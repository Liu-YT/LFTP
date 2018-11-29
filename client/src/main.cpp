#include "client.h"

int main(int argc, char *argv[]) {
    /*
    *   param
    *   op - lget or lsend
    *   server ip - default 127.0.0.1:8888
    *   file - default file dir is ../data
    */ 
    try {
        string op = argv[1];
        string serverIp = argv[2];
        string filename = argv[3];

        int index = serverIp.find_last_of(':');
        string host = serverIp.substr(0, index);
        // cout << host << endl;
        int port = atoi(serverIp.substr(index + 1).c_str());
        // cout << port << endl;

        if (op == "lget") {
            Client *client = new Client(host, filename, port);
            client->lget();
        }
        else if (op == "lsend") {
            Client *client = new Client(host, filename, port);
            client->lsend();
        }
        else {
            cout << "Please input as required";
            exit(0);
        }        
    } catch (exception &err) {
        cout << err.what() << endl;
        cout << "Please input as required";
        exit(1);
    }
    return 0;
}