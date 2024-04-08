#include "socketwrapper.h"

int socket_init()
{
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    return 0;
#endif
}

int socket_cleanup()
{
#ifdef _WIN32
    return WSACleanup();
#else
    return 0;
#endif
}