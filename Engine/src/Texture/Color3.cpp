#include "Texture/Color3.h"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <string>

template <class T>
T clamp(T value, T min, T max)
{
    return std::min(std::max(value, min), max);
}

Color3::Color3(float r, float g, float b)
{
    R = clamp(r, 0.0f, 1.0f);
    G = clamp(g, 0.0f, 1.0f);
    B = clamp(b, 0.0f, 1.0f);
}

Color3 Color3::FromRGB(float r, float g, float b)
{
    return { r / 255.0f, g / 255.0f, b / 255.0f };
}

Color3 Color3::FromHSV(int hue, int saturation, int value)
{
    float h = static_cast<float>(hue % 360);
    float s = clamp(static_cast<float>(saturation), 0.0f, 100.0f) / 100.0f;
    float v = clamp(static_cast<float>(value), 0.0f, 100.0f) / 100.0f;

    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r, g, b;

    if (0 <= h && h < 60) {
        r = c;
        g = x;
        b = 0;
    }
    else if (60 <= h && h < 120) {
        r = x;
        g = c;
        b = 0;
    }
    else if (120 <= h && h < 180) {
        r = 0;
        g = c;
        b = x;
    }
    else if (180 <= h && h < 240) {
        r = 0;
        g = x;
        b = c;
    }
    else if (240 <= h && h < 300) {
        r = x;
        g = 0;
        b = c;
    }
    else {
        r = c;
        g = 0;
        b = x;
    }

    return { r + m, g + m, b + m };
}

Color3 Color3::FromHex(std::string hexValue)
{
    if (hexValue[0] == '#') {
        hexValue = hexValue.substr(1);
    }

    if (hexValue.size() == 3) {
        hexValue = hexValue[0] + hexValue[0] + hexValue[1] + hexValue[1] + hexValue[2] + hexValue[2];
    }

    if (hexValue.size() != 6) {
        return { 0, 0, 0 };
    }

    int r = std::stoi(hexValue.substr(0, 2), nullptr, 16);  
    int g = std::stoi(hexValue.substr(2, 2), nullptr, 16);
    int b = std::stoi(hexValue.substr(4, 2), nullptr, 16);

    return { r / 255.0f, g / 255.0f, b / 255.0f };
}

Color3 Color3::Lerp(Color3 dest, float alpha)
{
    return (*this) * (1.0f - alpha) + (dest * alpha);
}

int Color3::ToHSV()
{
    std::cout << "Color3::ToHSV not implemented yet and return 0" << std::endl;
    return 0;
}

std::string Color3::ToHex()
{
    std::string result = "#";
    result += std::to_string(static_cast<int>(R * 255));
    result += std::to_string(static_cast<int>(G * 255));
    result += std::to_string(static_cast<int>(B * 255));

    return result;
}

// operator
Color3 Color3::operator+(Color3 const& color)
{
    return { this->R + color.R, this->G + color.G, this->B + color.B };
}

Color3 Color3::operator-(Color3 const& color)
{
    return { this->R - color.R, this->G - color.G, this->B - color.B };
}

Color3 Color3::operator*(Color3 const& color)
{
    return { this->R * color.R, this->G * color.G, this->B * color.B };
}

Color3 Color3::operator/(Color3 const& color)
{
    // Handle division by zero
    if (color.R == 0 || color.G == 0 || color.B == 0) {
        return { 0, 0, 0 };
    }
    return { this->R / color.R, this->G / color.G, this->B / color.B };
}

Color3 Color3::operator*(float const& value)
{
    return { this->R * value, this->G * value, this->B * value };
}
