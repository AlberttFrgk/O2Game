/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include <Logs.h>
#include <cstdarg>
#include <stdio.h>

#include <fmt/core.h>
#include <fmt/printf.h>

// #include "Console.h"

void Logs::Puts(const char *fmt, ...)
{
    char buffer[256] = {};

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    fmt::printf("%s\n", buffer);
}

void Logs::Debug(const char *fmt, ...)
{
#if defined(_DEBUG)
    char buffer[256] = {};

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    fmt::printf("%s\n", buffer);
#else
    (void)fmt;
#endif
}