/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#pragma once
#include <filesystem>
#include <vector>

namespace MusicListMaker {
    std::vector<std::filesystem::path> Prepare(std::filesystem::path path);
    void                               Insert(std::filesystem::path song_file);
} // namespace MusicListMaker