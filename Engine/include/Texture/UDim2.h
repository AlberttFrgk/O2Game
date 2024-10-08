#pragma once
#include "UDim.h"

class UDim2
{
public:
	UDim2();
	UDim2(UDim x, UDim y);
	UDim2(double xScale, double xOffset, double yScale, double yOffset);

	static UDim2 fromScale(double x, double y);
	static UDim2 fromOffset(double x, double y);

	UDim X;
	UDim Y;

	UDim2 Lerp(UDim2 destination, float alpha);

	UDim2 operator+ (UDim2 const& udim2);
	UDim2 operator- (UDim2 const& udim2);
	bool operator== (UDim2 const& udim2);
	bool operator<= (UDim2 const& udim2);
	bool operator>= (UDim2 const& udim2);
};