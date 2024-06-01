#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include "Util.hpp"
#include <algorithm>
#include <cstring>
#include <numeric>
#if _WIN32
#include <Windows.h>
#endif
#include <cerrno>
#include <cmath>
#include <codecvt>
#include <iconv.h>
#include <string>

std::vector<std::string> splitString(std::string &input, char delimeter)
{
    std::stringstream ss(input);
    return splitString(ss, delimeter);
}

std::vector<std::string> splitString(const std::string &input, char delimeter)
{
    std::stringstream ss(input);
    return splitString(ss, delimeter);
}

std::vector<std::string> splitString(std::stringstream &input, char delimiter)
{
    std::vector<std::string> result;
    std::string              line;
    while (std::getline(input, line, delimiter)) {
        result.push_back(line);
    }

    return result;
}

std::vector<std::string> splitString(std::stringstream &input)
{
    std::vector<std::string> result;
    std::string              line;
    while (std::getline(input, line)) {
        result.push_back(line);
    }

    return result;
}

std::string removeComment(const std::string &input)
{
    std::string result = input.substr(1);
    size_t      pos = result.find("//");
    if (pos != std::string::npos) {
        result = result.substr(0, pos);
    }

    return result;
}

std::string mergeVectorWith(std::vector<std::string> &vec, int starsAt)
{
    return std::accumulate(vec.begin() + starsAt, vec.end(), std::string{},
                           [](std::string acc, std::string str) {
                               return acc.empty() ? acc += str : acc += " " + str;
                           });
}

std::string mergeVector(std::vector<std::string> &vec, int starsAt)
{
    return std::accumulate(vec.begin() + starsAt, vec.end(), std::string{},
                           [](std::string acc, std::string str) {
                               return acc += str;
                           });
}

uint64_t Base36_Decode(const std::string &str)
{
    std::string CharList = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::string reversed = str;
    std::reverse(reversed.begin(), reversed.end());
    std::transform(reversed.begin(), reversed.end(), reversed.begin(), ::tolower);

    uint64_t result = 0;
    int      pos = 0;

    for (char c : reversed) {
        result += (uint64_t)(CharList.find(c) * pow(36, pos));
        pos++;
    }

    return result;
}

std::string Base36_Encode(uint64_t num)
{
    static const std::string digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::stringstream        ss;
    std::string              result;

    while (num > 0) {
        ss << digits[num % 36];
        num /= 36;
    }

    result = ss.str();
    std::reverse(result.begin(), result.end());

    if (result.length() == 1) {
        result = "0" + result;
    }

    return result;
}

void flipArray(uint8_t *arr, size_t size)
{
    uint8_t *start = arr;
    uint8_t *end = arr + size - 1;

    while (start < end) {
        // flip the values using XOR bitwise operation
        *start ^= *end;
        *end ^= *start;
        *start ^= *end;

        // move the pointers inward
        start++;
        end--;
    }
}

std::u8string CodepageToUtf8(const char* string, size_t str_len, const char* encoding) {
    // Open iconv conversion descriptor
    iconv_t conv = iconv_open("UTF-8", encoding);
    if (conv == (iconv_t)-1) {
        return u8"<encoding error>";
    }

    std::u8string result;
    size_t inbytesleft = str_len;
    const char* inbuf = string;
    size_t outbufsize = str_len * 4; // Initial output buffer size
    std::vector<char> outbuf(outbufsize);

    while (true) {
        char* outbufptr = outbuf.data();
        size_t outbytesleft = outbuf.size();

        // Reset errno to check for errors after iconv call
        errno = 0;

        size_t ret = iconv(conv, (char**)&inbuf, &inbytesleft, &outbufptr, &outbytesleft);
        if (ret != (size_t)-1 || errno == E2BIG) {
            // Successfully converted or need more space
            if (ret != (size_t)-1) {
                result.assign((const char8_t*)outbuf.data(), outbuf.size() - outbytesleft);
                break;
            }
            // Resize buffer and retry if buffer was too small
            size_t processed = outbuf.size() - outbytesleft;
            outbufsize *= 2;
            outbuf.resize(outbufsize);
            inbuf = string + (str_len - inbytesleft);
            std::fill(outbuf.begin() + processed, outbuf.end(), 0);
        }
        else {
            // Return whatever we have left, even if it's not valid
            result.assign((const char8_t*)outbuf.data());
            break;
        }
    }

    iconv_close(conv);
    return result;
}