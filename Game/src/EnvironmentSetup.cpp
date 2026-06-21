#include "EnvironmentSetup.hpp"
#include <mutex>
#include <unordered_map>


namespace {
struct EnvironmentData {
  std::unordered_map<std::string, std::string> stores;
  std::unordered_map<std::string, void *> storesPtr;
  std::unordered_map<std::string, std::filesystem::path> paths;
  std::unordered_map<std::string, int> ints;
  std::mutex mutex;
};

EnvironmentData &GetData() {
  static EnvironmentData data;
  return data;
}
} // namespace

void EnvironmentSetup::OnExitCheck() {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  data.stores.clear();
  data.paths.clear();
  data.ints.clear();
}

void EnvironmentSetup::Set(std::string key, std::string value) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  data.stores[key] = std::move(value);
}

std::string EnvironmentSetup::Get(std::string key) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  auto it = data.stores.find(key);
  if (it != data.stores.end()) {
    return it->second;
  }
  return "";
}

void EnvironmentSetup::SetObj(std::string key, void *ptr) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  data.storesPtr[key] = ptr;
}

void *EnvironmentSetup::GetObj(std::string key) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  auto it = data.storesPtr.find(key);
  if (it != data.storesPtr.end()) {
    return it->second;
  }
  return nullptr;
}

void EnvironmentSetup::SetPath(std::string key, std::filesystem::path path) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  data.paths[key] = std::move(path);
}

std::filesystem::path EnvironmentSetup::GetPath(std::string key) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  auto it = data.paths.find(key);
  if (it != data.paths.end()) {
    return it->second;
  }
  return {};
}

void EnvironmentSetup::SetInt(std::string key, int value) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  data.ints[key] = value;
}

int EnvironmentSetup::GetInt(std::string key) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  auto it = data.ints.find(key);
  if (it != data.ints.end()) {
    return it->second;
  }
  return 0;
}
