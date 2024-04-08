#include "LuaColor3.h"

#include <Math/Color3.h>

void LuaColor3::Register(sol::state &lua)
{
    auto color3_mul = sol::overload(
        static_cast<Color3 (Color3::*)(Color3 const &)>(&Color3::operator*),
        static_cast<Color3 (Color3::*)(float const &)>(&Color3::operator*));

    auto color3_div = sol::overload(
        static_cast<Color3 (Color3::*)(Color3 const &)>(&Color3::operator/),
        static_cast<Color3 (Color3::*)(float const &)>(&Color3::operator/));

    lua.new_usertype<Color3>("Color3",
                             sol::constructors<Color3(), Color3(float, float, float)>(),
                             "fromRGB", &Color3::fromRGB,
                             "fromHSV", &Color3::fromHSV,
                             "fromHex", &Color3::fromHex,
                             "Lerp", &Color3::Lerp,
                             "ToHSV", &Color3::ToHSV,
                             "ToHex", &Color3::ToHex,
                             "R", &Color3::R,
                             "G", &Color3::G,
                             "B", &Color3::B,
                             sol::meta_function::addition, &Color3::operator+,
                             sol::meta_function::subtraction, &Color3::operator-,
                             sol::meta_function::multiplication, color3_mul,
                             sol::meta_function::division, color3_div,
                             sol::meta_function::equal_to, &Color3::operator==,
                             sol::meta_function::less_than_or_equal_to, &Color3::operator<=,
                             sol::meta_function::less_than, &Color3::operator<);
}