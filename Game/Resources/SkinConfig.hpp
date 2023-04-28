#pragma once
#include <string>
#include <unordered_map>
#include <filesystem>

// Value Format: X, Y, AnchorPointX?, AnchorPointY?, TintColor?
struct PositionValue {
	int X, Y;
	float AnchorPointX, AnchorPointY;
	unsigned char RGB[3];
};

// Value format: X, Y, MaxDigits?, Direction?, FillWithZero?
struct NumericValue {
	int X, Y;
	int MaxDigit;
	int Direction;
	bool FillWithZero;
};

// Value format: X, Y, AnchorPointX, AnchorPointY
struct SpriteValue {
	int numOfFrames;
	int X, Y;
	float AnchorPointX, AnchorPointY;
};

class SkinConfig {
public:
	SkinConfig(std::string filePath, int keyCount);
	SkinConfig(std::filesystem::path path, int keyCount);
	~SkinConfig();

	PositionValue& GetPosition(std::string key);
	NumericValue& GetNumeric(std::string key);
	SpriteValue& GetSprite(std::string key);

private:
	void Load(std::filesystem::path path, int keyCount);

	std::unordered_map<std::string, PositionValue> m_positionValues;
	std::unordered_map<std::string, NumericValue> m_numericValues;
	std::unordered_map<std::string, SpriteValue> m_spriteValues;
};