#include "ShaderCache.h"
#include <fstream>
#include <chrono>
#include <iostream>

using namespace std::chrono;
namespace fs = std::filesystem;

ShaderCache::ShaderCache(const std::string& cacheDir) : cacheDir_(cacheDir) {
  // ensure dir exists
  try {
    fs::create_directories(cacheDir_);
  } catch (...) {}
}

ShaderCache::~ShaderCache() = default;

std::string ShaderCache::cacheDir() const { return cacheDir_; }

void ShaderCache::loadFromDisk() {
  std::lock_guard<std::mutex> lk(m_);
  try {
    for (auto &p : fs::directory_iterator(cacheDir_)) {
      if (!p.is_regular_file()) continue;
      auto path = p.path();
      if (path.extension() == ".bin") {
        try {
          auto entry = readEntryFromFile(path);
          map_.emplace(entry.key, std::move(entry));
        } catch (const std::exception& e) {
          std::cerr << "[ShaderCache] error reading " << path << ": " << e.what() << std::endl;
        }
      }
    }
    std::cerr << "[ShaderCache] loaded " << map_.size() << " entries from disk\n";
  } catch (const std::exception& e) {
    std::cerr << "[ShaderCache] loadFromDisk error: " << e.what() << std::endl;
  }
}

bool ShaderCache::tryGet(const std::string& key, std::vector<uint8_t>& outBlob) {
  std::lock_guard<std::mutex> lk(m_);
  auto it = map_.find(key);
  if (it == map_.end()) return false;
  outBlob = it->second.blob;
  std::cerr << "[ShaderCache] cache hit: " << key << " (size=" << outBlob.size() << ")\n";
  return true;
}

void ShaderCache::put(const std::string& key, const std::vector<uint8_t>& blob, const std::string& gpuId) {
  ShaderCacheEntry e;
  e.key = key;
  e.blob = blob;
  e.gpu_identifier = gpuId;
  e.timestamp = static_cast<uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());

  {
    std::lock_guard<std::mutex> lk(m_);
    map_[key] = e;
  }
  persistEntryToDisk(e);
  std::cerr << "[ShaderCache] put: " << key << " (size=" << blob.size() << ")\n";
}

void ShaderCache::persistEntryToDisk(const ShaderCacheEntry& e) {
  try {
    fs::create_directories(cacheDir_);
    fs::path binPath = fs::path(cacheDir_) / (e.key + ".bin");
    fs::path metaPath = fs::path(cacheDir_) / (e.key + ".meta");

    {
      std::ofstream out(binPath, std::ios::binary);
      out.write(reinterpret_cast<const char*>(e.blob.data()), static_cast<std::streamsize>(e.blob.size()));
    }
    {
      std::ofstream meta(metaPath);
      meta << e.gpu_identifier << "\n" << e.timestamp << "\n";
    }
  } catch (const std::exception& ex) {
    std::cerr << "[ShaderCache] persist error: " << ex.what() << std::endl;
  }
}

ShaderCacheEntry ShaderCache::readEntryFromFile(const std::filesystem::path& p) {
  ShaderCacheEntry e;
  e.key = p.stem().string(); // filename without extension
  // read blob
  std::ifstream in(p, std::ios::binary | std::ios::ate);
  if (!in) throw std::runtime_error("cannot open bin file");
  auto size = in.tellg();
  in.seekg(0);
  e.blob.resize(static_cast<size_t>(size));
  in.read(reinterpret_cast<char*>(e.blob.data()), size);

  // read metadata file
  fs::path meta = p.parent_path() / (e.key + ".meta");
  if (fs::exists(meta)) {
    std::ifstream m(meta);
    if (m) {
      std::getline(m, e.gpu_identifier);
      std::string ts;
      std::getline(m, ts);
      if (!ts.empty()) e.timestamp = std::stoull(ts);
    }
  }
  return e;
}
