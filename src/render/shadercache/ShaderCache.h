#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <filesystem>

struct ShaderCacheEntry {
  std::string key;
  std::vector<uint8_t> blob;
  std::string gpu_identifier;
  uint64_t timestamp;
};

class ShaderCache {
public:
  explicit ShaderCache(const std::string& cacheDir);
  ~ShaderCache();

  // load persisted entries metadata (called at startup)
  void loadFromDisk();

  // try to fetch compiled blob (returns true if found and set outBlob)
  bool tryGet(const std::string& key, std::vector<uint8_t>& outBlob);

  // put compiled blob into cache and persist to disk
  void put(const std::string& key, const std::vector<uint8_t>& blob, const std::string& gpuId);

  // simple helper: returns cache dir
  std::string cacheDir() const;

private:
  void persistEntryToDisk(const ShaderCacheEntry& e);
  ShaderCacheEntry readEntryFromFile(const std::filesystem::path& p);

  std::string cacheDir_;
  std::unordered_map<std::string, ShaderCacheEntry> map_;
  std::mutex m_;
};
