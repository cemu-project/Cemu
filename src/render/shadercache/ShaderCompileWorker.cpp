#include "ShaderCompileWorker.h"
#include <iostream>
#include <chrono>
#include <thread>

// A tiny "fake" compile function to simulate compilation work.
// Replace this with actual renderer compile in the integration step.
static bool FakeCompileToBlob(const std::string& src, std::vector<uint8_t>& out) {
  // Simulate some work and produce a small blob
  std::this_thread::sleep_for(std::chrono::milliseconds(150)); // simulate compile latency
  out.assign(src.begin(), src.end()); // just copy source bytes as a stand-in
  return true;
}

ShaderCompileWorker::ShaderCompileWorker(ShaderCache& cache) : cache_(cache) {}
ShaderCompileWorker::~ShaderCompileWorker() { stop(); }

void ShaderCompileWorker::start() {
  stop_.store(false);
  worker_ = std::thread([this]{ threadMain(); });
}

void ShaderCompileWorker::stop() {
  stop_.store(true);
  // push a dummy to wake up
  queue_.push(CompileTask{});
  if (worker_.joinable()) worker_.join();
}

void ShaderCompileWorker::enqueue(CompileTask task) {
  queue_.push(std::move(task));
}

void ShaderCompileWorker::threadMain() {
  while (true) {
    auto opt = queue_.pop_wait(stop_);
    if (!opt.has_value()) break;
    CompileTask task = std::move(*opt);
    if (task.key.empty()) continue; // ignore wake-up dummy

    // Double-check cache to avoid duplicate work
    std::vector<uint8_t> existing;
    if (cache_.tryGet(task.key, existing)) continue;

    std::vector<uint8_t> compiled;
    bool ok = FakeCompileToBlob(task.source, compiled); // TODO replace with real compile

    if (ok) {
      cache_.put(task.key, compiled, task.gpuId);
      std::cerr << "[ShaderWorker] compiled key=" << task.key << " size=" << compiled.size() << "\n";
      // Optionally: enqueue a main-thread action to create the GPU shader object from compiled blob.
      // This must be executed on the render thread because many APIs require a valid context.
    } else {
      std::cerr << "[ShaderWorker] compile failed for key=" << task.key << "\n";
    }
  }
}
