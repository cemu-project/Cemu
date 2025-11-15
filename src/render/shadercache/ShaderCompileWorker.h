#pragma once
#include "ThreadSafeQueue.h"
#include "ShaderCache.h"
#include <thread>
#include <atomic>
#include <string>
#include <vector>

struct CompileTask {
  std::string key;        // hash of source + options
  std::string source;     // shader source (or path to IR)
  std::string gpuId;      // GPU id used for this compile
};

class ShaderCompileWorker {
public:
  explicit ShaderCompileWorker(ShaderCache& cache);
  ~ShaderCompileWorker();

  void start();
  void stop();

  void enqueue(CompileTask task);

private:
  void threadMain();
  ThreadSafeQueue<CompileTask> queue_;
  std::thread worker_;
  std::atomic<bool> stop_{false};
  ShaderCache& cache_;
};
