#ifndef __LUA_MATH_H

#include <sol/sol.hpp>

namespace LuaMath {
    double Pow(double x, double y);
    double Sqrt(double x);
    double Sin(double x);
    double Cos(double x);
    double Tan(double x);
    double Asin(double x);
    double Acos(double x);
    double Atan(double x);
    double Atan2(double y, double x);
    double Abs(double x);
    double Ceil(double x);
    double Floor(double x);
    double Round(double x);
    double Log(double x);
    double Log10(double x);
    double Lerp(double a, double b, double t);
    double Min(double a, double b);
    double Max(double a, double b);
    double Clamp(double x, double min, double max);

    void Register(sol::state &lua);
}; // namespace LuaMath

#endif // __LUA_MATH_H_