#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "socketwrapper.h"

#include <functional>
#include <string>
#include <thread>
#include <vector>

class Client
{
public:
    Client() = default;
    Client(std::string IPAddress, int port);

    bool Connect();
    void Disconnect();

    void                 Send(const std::vector<uint8_t> &data);
    void                 OnResponse(std::function<void(std::vector<uint8_t>)> callback);
    std::vector<uint8_t> GetResponse();

private:
    SOCKET      m_ClientSocket;
    std::string m_IPAddress;
    int         m_Port;

    bool m_GotResponse = false;
    bool m_Running = false;

    std::thread          m_ClientThread;
    std::vector<uint8_t> m_Response;

    std::function<void(std::vector<uint8_t>)> m_OnResponse;
};

#endif