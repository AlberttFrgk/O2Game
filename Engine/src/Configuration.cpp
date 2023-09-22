#include "Configuration.h"
#include <iostream>
#include "Misc/mINI.h"

namespace {
    bool IsLoaded = false;
	mINI::INIStructure Config;

	std::string CurrentSkin = "";
	mINI::INIStructure SkinConfig;

	std::string defaultConfig;
}

void Configuration::SetDefaultConfiguration(std::string conf) {
	defaultConfig = conf;
}

void Configuration::ResetConfiguration() {
	std::filesystem::path path = std::filesystem::current_path() / "Game.ini";

	if (std::filesystem::exists(path)) {
		std::cout << "Deleting configuration at path: " << path.string() << std::endl;
		std::filesystem::remove(path);
	}

	{
		std::fstream fs(path, std::ios::out);
		fs << defaultConfig;
		fs.close();
	}

	mINI::INIFile file(path);
	file.read(Config);

	IsLoaded = true;
}

void LoadConfiguration() {
	if (IsLoaded) return;

	std::filesystem::path path = std::filesystem::current_path() / "Game.ini";

	if (!std::filesystem::exists(path)) {
		std::cout << "Creating default configuration at path: " << path.string() << std::endl;

		std::fstream fs(path, std::ios::out);
		fs << defaultConfig;
		fs.close();
	}

	mINI::INIFile file(path);
	file.read(Config);

	IsLoaded = true;
}

std::string Configuration::Load(std::string key, std::string prop) {
	if (!IsLoaded) LoadConfiguration();

	return Config[key][prop];
}

void Configuration::Set(std::string key, std::string prop, std::string value) {
	if (!IsLoaded) LoadConfiguration();

	Config[key][prop] = value;

	std::filesystem::path path = std::filesystem::current_path() / "Game.ini";
	
	mINI::INIFile file(path);
	if (!file.write(Config, true)) {
		std::cout << "Failed to write configuration to file" << std::endl;
	}
}

void Configuration::Skin_Load(std::string name) {
	CurrentSkin = name;
}

std::filesystem::path Configuration::Skin_GetPath() {
	return std::filesystem::current_path() / "Skins" / CurrentSkin;
}

std::string Configuration::Skin_LoadValue(std::string key, std::string prop) {
	mINI::INIStructure skinConfig;
	mINI::INIFile file(Skin_GetPath() / "GameSkin.ini");
	file.read(skinConfig);

	return skinConfig[key][prop];
}

bool Configuration::Skin_Exist(std::string name) {
	std::filesystem::path path = std::filesystem::current_path() / "Skins" / name;
	return std::filesystem::exists(path);
}