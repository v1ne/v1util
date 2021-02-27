
#include "filesystem.hpp"

#include <string>
#include <system_error>

namespace v1util {

namespace {
constexpr const auto kThisFile = "stl-plus/filesystem.cpp";
constexpr const auto kTestFilesDir = "test-files";
}  // namespace

std::filesystem::path repoPath() {
  static auto p = []() {
    std::error_code ec;
    std::filesystem::path path = std::filesystem::current_path(ec);
    if(ec) return std::filesystem::path{};

    std::filesystem::path oldPath;
    while(oldPath != path && !(std::filesystem::exists(kThisFile, ec) && !ec)) {
      if(std::filesystem::exists((path / "v1util" / kThisFile), ec) && !ec) return path / "v1util";
      if(std::filesystem::exists((path / "third-party/v1util" / kThisFile), ec) && !ec)
        return path / "v1util";
      oldPath = path;
      path = path.parent_path();
    }

    return path == oldPath ? std::filesystem::path{} : path;
  }();

  return p;
}

std::filesystem::path testFilesPath() {
  static auto p = []() {
    auto path = repoPath();
    return path.empty() ? path : path / kTestFilesDir;
  }();

  return p;
}

std::filesystem::path unique_path(std::filesystem::path prefix, std::string_view suffix) {
  if(prefix.empty()) return {};

  for(int count = 0; count < 1'000'000; ++count) {
    auto path = prefix;
    if(count == 0)
      path.concat(suffix);
    else
      path.concat("-" + (std::to_string(count)) + (!suffix.empty() && suffix[0] != '.' ? "-" : ""))
          .concat(suffix);
    std::error_code ec;
    if(!exists(path, ec) && !ec) return path;
  }

  return {};
}

}  // namespace v1util
