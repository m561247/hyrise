#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <vector>
#include "benchmark/benchmark.h"
#include "buffer_benchmark_utils.hpp"
#include "storage/buffer/buffer_manager.hpp"

namespace hyrise {
class SkewFixture : public benchmark::Fixture {
 public:
  constexpr static auto PAGE_SIZE_TYPE = MIN_PAGE_SIZE_TYPE;

  constexpr static auto NUM_OPERATIONS = 100000000;

  YCSBTable table;
  YCSBOperations operations;
  BufferManager& buffer_manager = Hyrise::get().buffer_manager;
  uint64_t operations_per_thread;

  void SetUp(const ::benchmark::State& state) {
    if (state.thread_index() == 0) {
      auto config = BufferManager::Config::from_env();
      config.dram_buffer_pool_size = 1UL * GB;
      config.numa_buffer_pool_size = 0;
      config.cpu_node = NodeID{0};
      config.enable_numa = false;

      Hyrise::get().buffer_manager = BufferManager(config);

      auto database_size = 2UL * GB;
      auto skew = (double)state.range(0) / double(1000);
      table = generate_ycsb_table(&buffer_manager, database_size);
      operations = generate_ycsb_operations<YCSBWorkload::ReadMostly, NUM_OPERATIONS>(table.size(), skew);
      operations_per_thread = operations.size() / state.threads();
      buffer_manager.metrics()->total_hits = 0;
      buffer_manager.metrics()->total_misses = 0;
    }
  }
};

BENCHMARK_DEFINE_F(SkewFixture, BM_Skew)(benchmark::State& state) {
  for (auto _ : state) {
    const auto start = state.thread_index() * operations_per_thread;
    const auto end = start + operations_per_thread;
    for (auto i = start; i < end; ++i) {
      const auto op = operations[i];
      const auto timer_start = std::chrono::high_resolution_clock::now();
      auto bytes = execute_ycsb_action(table, buffer_manager, op);
      benchmark::DoNotOptimize(bytes);
    }
  }

  if (state.thread_index() == 0) {
    state.counters["cache_hit_rate"] = buffer_manager.metrics()->hit_rate();
    state.counters["total_hits"] = buffer_manager.metrics()->total_hits.load();
    state.counters["total_misses"] = buffer_manager.metrics()->total_misses.load();
  }
  state.SetItemsProcessed(operations_per_thread);
}

// Args are divided by 1000 to be passed to zipf generator
BENCHMARK_REGISTER_F(SkewFixture, BM_Skew)
    ->Threads(4)
    ->Iterations(1)
    ->Arg(1)
    ->Arg(100)
    ->Arg(200)
    ->Arg(500)
    ->Arg(700)
    ->Arg(800)
    ->Arg(900)
    ->Arg(990)
    ->Arg(999)
    ->Name("BM_skew");
}  // namespace hyrise