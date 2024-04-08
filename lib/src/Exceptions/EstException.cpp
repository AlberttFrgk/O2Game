/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include <Exceptions/EstException.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <cstdarg>
#include <array>
using namespace Exceptions;

Exceptions::EstException::EstException()
{
    m_Message = "Unknown exception";
}

EstException::EstException(const char *message, ...)
{
    std::array<char, 1024> buffer = { 0 };

    va_list args;
    va_start(args, message);

    vsnprintf(buffer.data(), buffer.size(), message, args);

    va_end(args);

    m_Message = buffer.data();
}

EstException::~EstException() throw()
{
}

const char *EstException::what() const throw()
{
    return m_Message.c_str();
}
