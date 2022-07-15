#include "webserver.h"
int main(int argc, char *argv[])
{
    WebServer server(argc,argv);

    server.runEnvironment_init();

    server.mainStart();
    
    return 0;
}