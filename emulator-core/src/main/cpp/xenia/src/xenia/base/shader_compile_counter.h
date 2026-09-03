#ifndef XENIA_BASE_SHADER_COMPILE_COUNTER_H_
#define XENIA_BASE_SHADER_COMPILE_COUNTER_H_
#include <atomic>
#include <cstdint>
namespace xe {
// Number of GPU pipelines/shaders currently being created (in-flight).
// Today this is 0 or 1 (synchronous creation on the command-processor thread);
// it becomes >1 once async pipeline creation lands. Always-on, trivially cheap.
inline std::atomic<uint32_t>& shader_compiles_in_flight() {
  static std::atomic<uint32_t> counter{0};
  return counter;
}
inline uint32_t shader_compiles_in_flight_count() {
  return shader_compiles_in_flight().load(std::memory_order_relaxed);
}
// RAII guard: increment on construction, decrement on destruction. Use around
// each pipeline/shader create so it survives early returns.
class ScopedShaderCompile {
 public:
  ScopedShaderCompile() {
    shader_compiles_in_flight().fetch_add(1, std::memory_order_relaxed);
  }
  ~ScopedShaderCompile() {
    shader_compiles_in_flight().fetch_sub(1, std::memory_order_relaxed);
  }
  ScopedShaderCompile(const ScopedShaderCompile&) = delete;
  ScopedShaderCompile& operator=(const ScopedShaderCompile&) = delete;
};
}  // namespace xe
#endif  // XENIA_BASE_SHADER_COMPILE_COUNTER_H_
