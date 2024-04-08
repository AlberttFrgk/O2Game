/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "GameCoordinator.h"
#include <Exceptions/EstException.h>

namespace {
    std::string ServerAddress = "127.0.0.1";
    int         ServerPort = 45510;
} // namespace

GameCoordinator::GameCoordinator()
{
    if (socket_init() != 0) {
        throw Exceptions::EstException("Failed to initialize socket library");
    }
}

GameCoordinator::~GameCoordinator()
{
    socket_cleanup();
}

bool GameCoordinator::Connect()
{
    m_client = std::make_shared<Client>(ServerAddress, ServerPort);
    m_client->OnResponse([this](std::vector<uint8_t> data) {
        uint32_t packetSize = *reinterpret_cast<uint32_t *>(data.data());
        uint32_t opcode = *reinterpret_cast<uint32_t *>(data.data() + 4);

        if (m_handlers.find(opcode) != m_handlers.end()) {
            std::vector<uint8_t> packetData(data.begin() + 8, data.end());
            m_handlers[opcode](packetData);
        }
    });

    if (m_client->Connect()) {
        return true;
    }

    m_client.reset();
    return false;
}

void GameCoordinator::Disconnect()
{
    if (m_client) {
        m_client->Disconnect();
    }

    m_client.reset();
}

void GameCoordinator::OnData(uint32_t opcode, std::function<void(std::vector<uint8_t> data)> handler)
{
    m_handlers[opcode] = handler;
}

void GameCoordinator::Send(uint32_t opcode, std::vector<uint8_t> data)
{
    std::vector<uint8_t> packet;
    packet.resize(data.size() + 8);

    *reinterpret_cast<uint32_t *>(packet.data()) = (int)(data.size() + 4);
    *reinterpret_cast<uint32_t *>(packet.data() + 4) = opcode;
    std::copy(data.begin(), data.end(), packet.begin() + 8);

    m_client->Send(packet);
}

GameCoordinator *GameCoordinator::m_instance = nullptr;

GameCoordinator *GameCoordinator::Get()
{
    if (!m_instance) {
        m_instance = new GameCoordinator();
    }

    return m_instance;
}

void GameCoordinator::Destroy()
{
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}