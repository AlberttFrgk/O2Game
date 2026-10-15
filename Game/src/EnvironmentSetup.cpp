#include "EnvironmentSetup.hpp"
#include <mutex>
#include <string_view>
#include <unordered_map>

namespace {
struct StringViewHash {
  using is_transparent = void;
  size_t operator()(std::string_view sv) const noexcept {
    return std::hash<std::string_view>{}(sv);
  }
};

struct EnvironmentData {
  std::unordered_map<std::string, std::string, StringViewHash, std::equal_to<>> stores;
  std::unordered_map<std::string, void *, StringViewHash, std::equal_to<>> storesPtr;
  std::unordered_map<std::string, std::filesystem::path, StringViewHash, std::equal_to<>> paths;
  std::unordered_map<std::string, int, StringViewHash, std::equal_to<>> ints;
  std::mutex mutex;
};

EnvironmentData &GetData() {
  static auto *data = new EnvironmentData();
  return *data;
}
} // namespace

void EnvironmentSetup::OnExitCheck() {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  data.stores.clear();
  data.paths.clear();
  data.ints.clear();
  data.storesPtr.clear();
}

void EnvironmentSetup::Set(std::string_view key, std::string value) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  data.stores.insert_or_assign(std::string(key), std::move(value));
}

std::string EnvironmentSetup::Get(std::string_view key) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  auto it = data.stores.find(key);
  if (it != data.stores.end()) {
    return it->second;
  }
  return "";
}

void EnvironmentSetup::SetObj(std::string_view key, void *ptr) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  data.storesPtr.insert_or_assign(std::string(key), ptr);
}

void *EnvironmentSetup::GetObj(std::string_view key) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  auto it = data.storesPtr.find(key);
  if (it != data.storesPtr.end()) {
    return it->second;
  }
  return nullptr;
}

void EnvironmentSetup::SetPath(std::string_view key, std::filesystem::path path) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  data.paths.insert_or_assign(std::string(key), std::move(path));
}

std::filesystem::path EnvironmentSetup::GetPath(std::string_view key) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  auto it = data.paths.find(key);
  if (it != data.paths.end()) {
    return it->second;
  }
  return {};
}

void EnvironmentSetup::SetInt(std::string_view key, int value) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  data.ints.insert_or_assign(std::string(key), value);
}

int EnvironmentSetup::GetInt(std::string_view key) {
  auto &data = GetData();
  std::lock_guard<std::mutex> lock(data.mutex);
  auto it = data.ints.find(key);
  if (it != data.ints.end()) {
    return it->second;
  }
  return 0;
}
