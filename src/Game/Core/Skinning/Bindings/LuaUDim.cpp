#include "LuaUDim.h"

#include <Math/UDim.h>

void LuaUDim::Register(sol::state &lua)
{
    lua.new_usertype<UDim>("UDim",
                           sol::constructors<UDim(), UDim(double, double)>(),
                           "Scale", &UDim::Scale,
                           "Offset", &UDim::Offset,
                           "Lerp", &UDim::Lerp,
                           sol::meta_function::addition, &UDim::operator+,
                           sol::meta_function::subtraction, &UDim::operator-,
                           sol::meta_function::multiplication, &UDim::operator*,
                           sol::meta_function::division, &UDim::operator/,
                           sol::meta_function::equal_to, &UDim::operator==,
                           sol::meta_function::less_than_or_equal_to, &UDim::operator<=,
                           sol::meta_function::less_than, &UDim::operator<);
}