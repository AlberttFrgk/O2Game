/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#ifndef __GAME_COORDINATOR_H
#define __GAME_COORDINATOR_H

#include "socket/client.h"
#include <map>

class GameCoordinator
{
public:
    static GameCoordinator *Get();
    static void             Destroy();

    bool Connect();
    void Disconnect();

    void OnData(uint32_t opcode, std::function<void(std::vector<uint8_t> data)> handler);
    void Send(uint32_t opcode, std::vector<uint8_t> data);

private:
    GameCoordinator();
    ~GameCoordinator();

    std::shared_ptr<Client>                                            m_client;
    std::map<uint32_t, std::function<void(std::vector<uint8_t> data)>> m_handlers;

    static GameCoordinator *m_instance;
};

#endif