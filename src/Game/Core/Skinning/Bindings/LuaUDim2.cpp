#include "LuaUDim2.h"

#include <Math/UDim2.h>

void LuaUDim2::Register(sol::state &lua)
{
    lua.new_usertype<UDim2>("UDim2",
                            sol::constructors<UDim2(), UDim2(UDim, UDim), UDim2(double, double, double, double)>(),
                            "X", &UDim2::X,
                            "Y", &UDim2::Y,
                            "Lerp", &UDim2::Lerp,
                            "fromOffset", &UDim2::fromOffset,
                            "fromScale", &UDim2::fromScale,
                            sol::meta_function::addition, &UDim2::operator+,
                            sol::meta_function::subtraction, &UDim2::operator-,
                            sol::meta_function::multiplication, &UDim2::operator*,
                            sol::meta_function::division, &UDim2::operator/,
                            sol::meta_function::equal_to, &UDim2::operator==,
                            sol::meta_function::less_than_or_equal_to, &UDim2::operator<=,
                            sol::meta_function::less_than, &UDim2::operator<);
}