#include "LuaMath.h"

double LuaMath::Pow(double x, double y)
{
    return pow(x, y);
}

double LuaMath::Sqrt(double x)
{
    return sqrt(x);
}

double LuaMath::Sin(double x)
{
    return sin(x);
}

double LuaMath::Cos(double x)
{
    return cos(x);
}

double LuaMath::Tan(double x)
{
    return tan(x);
}

double LuaMath::Asin(double x)
{
    return asin(x);
}

double LuaMath::Acos(double x)
{
    return acos(x);
}

double LuaMath::Atan(double x)
{
    return atan(x);
}

double LuaMath::Atan2(double y, double x)
{
    return atan2(y, x);
}

double LuaMath::Abs(double x)
{
    return abs(x);
}

double LuaMath::Ceil(double x)
{
    return ceil(x);
}

double LuaMath::Floor(double x)
{
    return floor(x);
}

double LuaMath::Round(double x)
{
    return round(x);
}

double LuaMath::Log(double x)
{
    return log(x);
}

double LuaMath::Log10(double x)
{
    return log10(x);
}

double LuaMath::Lerp(double a, double b, double t)
{
    return a + (b - a) * t;
}

double LuaMath::Min(double a, double b)
{
    return std::min(a, b);
}

double LuaMath::Max(double a, double b)
{
    return std::max(a, b);
}

double LuaMath::Clamp(double x, double min, double max)
{
    return std::clamp(x, min, max);
}

void LuaMath::Register(sol::state &lua)
{
    sol::table math = lua.create_named_table("Math");
    math.set_function("pow", sol::c_call<decltype(&LuaMath::Pow), &LuaMath::Pow>);
    math.set_function("sqrt", sol::c_call<decltype(&LuaMath::Sqrt), &LuaMath::Sqrt>);
    math.set_function("sin", sol::c_call<decltype(&LuaMath::Sin), &LuaMath::Sin>);
    math.set_function("cos", sol::c_call<decltype(&LuaMath::Cos), &LuaMath::Cos>);
    math.set_function("tan", sol::c_call<decltype(&LuaMath::Tan), &LuaMath::Tan>);
    math.set_function("asin", sol::c_call<decltype(&LuaMath::Asin), &LuaMath::Asin>);
    math.set_function("acos", sol::c_call<decltype(&LuaMath::Acos), &LuaMath::Acos>);
    math.set_function("atan", sol::c_call<decltype(&LuaMath::Atan), &LuaMath::Atan>);
    math.set_function("atan2", sol::c_call<decltype(&LuaMath::Atan2), &LuaMath::Atan2>);
    math.set_function("abs", sol::c_call<decltype(&LuaMath::Abs), &LuaMath::Abs>);
    math.set_function("ceil", sol::c_call<decltype(&LuaMath::Ceil), &LuaMath::Ceil>);
    math.set_function("floor", sol::c_call<decltype(&LuaMath::Floor), &LuaMath::Floor>);
    math.set_function("round", sol::c_call<decltype(&LuaMath::Round), &LuaMath::Round>);
    math.set_function("log", sol::c_call<decltype(&LuaMath::Log), &LuaMath::Log>);
    math.set_function("log10", sol::c_call<decltype(&LuaMath::Log10), &LuaMath::Log10>);
    math.set_function("lerp", sol::c_call<decltype(&LuaMath::Lerp), &LuaMath::Lerp>);
    math.set_function("min", sol::c_call<decltype(&LuaMath::Min), &LuaMath::Min>);
    math.set_function("max", sol::c_call<decltype(&LuaMath::Max), &LuaMath::Max>);
    math.set_function("clamp", sol::c_call<decltype(&LuaMath::Clamp), &LuaMath::Clamp>);

    math["pi"] = 3.14159265358979323846f;
    math["e"] = 2.71828182845904523536f;

    lua["math"] = math;
}