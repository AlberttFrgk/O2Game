#include "LuaVector2.h"

#include <Math/Vector2.h>

void LuaVector2::Register(sol::state &lua)
{
    auto vector2_mult = sol::overload(
        [](Vector2 &v1, Vector2 const &v2) { return v1 * v2; },
        [](Vector2 &v1, double const &v2) { return v1 * v2; });

    auto vector2_div = sol::overload(
        [](Vector2 &v1, Vector2 const &v2) { return v1 / v2; },
        [](Vector2 &v1, double const &v2) { return v1 / v2; });

    lua.new_usertype<Vector2>("Vector2",
                              sol::constructors<Vector2(), Vector2(double, double)>(),
                              "Cross", &Vector2::Cross,
                              "Dot", &Vector2::Dot,
                              "Lerp", &Vector2::Lerp,
                              sol::meta_function::addition, &Vector2::operator+,
                              sol::meta_function::subtraction, &Vector2::operator-,
                              sol::meta_function::multiplication, vector2_mult,
                              sol::meta_function::division, vector2_div,
                              sol::meta_function::equal_to, &Vector2::operator==);
}