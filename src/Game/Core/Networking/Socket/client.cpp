#include "client.h"

#include <chrono>
#include <iostream>

const int kMaxBufferSize = 1024 * 16;

Client::Client(std::string IPAddress, int Port)
{
    m_IPAddress = IPAddress;
    m_Port = Port;
}

bool Client::Connect()
{
    int iResult;

    struct addrinfo *result = nullptr;
    struct addrinfo  hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo(m_IPAddress.c_str(), std::to_string(m_Port).c_str(), &hints, &result);
    if (iResult != 0) {
        std::cerr << "Error: getaddrinfo failed with error: " << iResult << std::endl;
        return false;
    }

    m_ClientSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (m_ClientSocket == -1) {
        std::cerr << "Error: Could not create socket" << std::endl;
        freeaddrinfo(result);
        return false;
    }

    iResult = connect(m_ClientSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == -1) {
        std::cerr << "Error: Could not connect to server" << std::endl;
        freeaddrinfo(result);
        return false;
    }

    freeaddrinfo(result);
    m_Running = true;

    m_ClientThread = std::thread([this]() {
        while (m_Running) {
            char buffer[kMaxBufferSize];
            int  bytesRead = recv(m_ClientSocket, buffer, kMaxBufferSize, 0);

            if (bytesRead > 0) {
                std::vector<uint8_t> data(buffer, buffer + bytesRead);
                m_GotResponse = true;
                m_Response = data;

                if (m_OnResponse) {
                    m_OnResponse(data);
                }
            }
        }
    });

    return true;
}

void Client::Disconnect()
{
    m_Running = false;
    m_ClientThread.join();

#if defined(_WIN32)
    closesocket(m_ClientSocket);
#else
    close(m_ClientSocket);
#endif
}

void Client::Send(const std::vector<uint8_t> &data)
{
    m_GotResponse = false;

    send(m_ClientSocket, reinterpret_cast<const char *>(data.data()), (int)data.size(), 0);
}

void Client::OnResponse(std::function<void(std::vector<uint8_t>)> callback)
{
    m_OnResponse = callback;
}

std::vector<uint8_t> Client::GetResponse()
{
    while (!m_GotResponse) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return m_Response;
}
