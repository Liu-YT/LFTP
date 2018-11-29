#include "server.h"

int main() {
    Server* server = new Server();
    server->waitForClient();
    return 0;
}