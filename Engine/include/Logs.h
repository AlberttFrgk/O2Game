#pragma once

namespace Logs {
    void Puts(const char* fmt, ...);
    void Write(const char* tag, const char* fmt, ...);
    void Flush();
}