#include <memory>
#include <new>
#include <vector>

#include <random>
#include "benchmark/benchmark.h"
#include "buffer_benchmark_utils.hpp"
#include "hdr/hdr_histogram.h"
#include "hyrise.hpp"
#include "storage/buffer/buffer_manager.hpp"
#include "storage/buffer/jemalloc_resource.hpp"
#include "storage/buffer/zipfian_int_distribution.hpp"

namespace hyrise {

/**
 * Bechmark idea:
 * Sacle read ops with difedderne page size
 * Scale read ops with single page size and different DRAM size ratios
 * Use zipfian skews to test different hit and miss rates for single pahe size and diffeent dram size ratios
 * 
 * Partly taken from https://github.com/hpides/viper/tree/master
 * 
*/
template <YCSBWorkload WL, MigrationPolicy policy = LazyMigrationPolicy>
class YCSBBufferManagerFixture : public benchmark::Fixture {
 public:
  constexpr static auto PAGE_SIZE_TYPE = MIN_PAGE_SIZE_TYPE;
  constexpr static auto DEFAULT_DRAM_BUFFER_POOL_SIZE = 2UL * GB;
  constexpr static auto DEFAULT_NUMA_BUFFER_POOL_SIZE = 4UL * GB;

  constexpr static auto NUM_OPERATIONS = 1000000;

  YCSBTable table;
  YCSBOperations operations;
  hdr_histogram* latency_histogram;
  std::mutex latency_histogram_mutex;
  BufferManager& buffer_manager = Hyrise::get().buffer_manager;
  uint64_t operations_per_thread;

  void SetUp(const ::benchmark::State& state) {
    if (state.thread_index() == 0) {
      auto config = BufferManager::Config::from_env();
      config.dram_buffer_pool_size = DEFAULT_DRAM_BUFFER_POOL_SIZE;
      config.numa_buffer_pool_size = DEFAULT_NUMA_BUFFER_POOL_SIZE;
      config.migration_policy = policy;
      config.enable_numa = (policy != DramOnlyMigrationPolicy);

      Hyrise::get().buffer_manager = BufferManager(config);

      auto database_size = state.range(0) * GB;
      table = generate_ycsb_table(&buffer_manager, database_size);
      operations = generate_ycsb_operations<WL, NUM_OPERATIONS>(table.size(), 0.9);
      operations_per_thread = operations.size() / state.threads();
      init_histogram(&latency_histogram);
    }
  }

  void TearDown(const ::benchmark::State& state) {
    if (state.thread_index() == 0) {
      hdr_close(latency_histogram);
    }
  }

  void warmup(benchmark::State& state) {
    auto start = state.thread_index() * operations_per_thread;
    auto end = start + operations_per_thread;
    for (auto i = start; i < end; ++i) {
      const auto op = operations[i];
      execute_ycsb_action(table, buffer_manager, op);
    }
  }
};

template <typename Fixture>
inline void run_ycsb(Fixture& fixture, benchmark::State& state) {
  micro_benchmark_clear_cache();

  auto bytes_processed = uint64_t{0};

  hdr_histogram* local_latency_histogram;
  init_histogram(&local_latency_histogram);

  for (auto _ : state) {
    const auto start = state.thread_index() * fixture.operations_per_thread;
    const auto end = start + fixture.operations_per_thread;
    for (auto i = start; i < end; ++i) {
      const auto op = fixture.operations[i];
      const auto timer_start = std::chrono::high_resolution_clock::now();
      bytes_processed += execute_ycsb_action(fixture.table, fixture.buffer_manager, op);
      const auto timer_end = std::chrono::high_resolution_clock::now();
      const auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(timer_end - timer_start).count();
      hdr_record_value(local_latency_histogram, latency);
    }
    {
      std::lock_guard<std::mutex> lock{fixture.latency_histogram_mutex};
      hdr_add(fixture.latency_histogram, local_latency_histogram);
      hdr_close(local_latency_histogram);
    }
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(fixture.operations_per_thread);
  state.SetBytesProcessed(bytes_processed);

  if (state.thread_index() == 0) {
    state.counters["cache_hit_rate"] = fixture.buffer_manager.metrics()->hit_rate();
    state.counters["latency_mean"] = hdr_mean(fixture.latency_histogram);
    state.counters["latency_stddev"] = hdr_stddev(fixture.latency_histogram);
    state.counters["latency_median"] = hdr_value_at_percentile(fixture.latency_histogram, 50.0);
    state.counters["latency_min"] = hdr_min(fixture.latency_histogram);
    state.counters["latency_max"] = hdr_max(fixture.latency_histogram);
    state.counters["latency_95percentile"] = hdr_value_at_percentile(fixture.latency_histogram, 95.0);
  }
}

#define CONFIGURE_BENCHMARK(WL, Policy)                                                                 \
  BENCHMARK_TEMPLATE_DEFINE_F(YCSBBufferManagerFixture, BM_ycsb_##WL##Policy, YCSBWorkload::WL, Policy) \
  (benchmark::State & state) {                                                                          \
    run_ycsb(*this, state);                                                                             \
  }                                                                                                     \
  BENCHMARK_REGISTER_F(YCSBBufferManagerFixture, BM_ycsb_##WL##Policy)                                  \
      ->ThreadRange(1, 48)                                                                              \
      ->Iterations(1)                                                                                   \
      ->Repetitions(1)                                                                                  \
      ->UseRealTime()                                                                                   \
      ->DenseRange(1, 8, 1)                                                                             \
      ->Name("BM_ycsb/" #WL "/" #Policy);

CONFIGURE_BENCHMARK(UpdateHeavy, LazyMigrationPolicy)
CONFIGURE_BENCHMARK(ReadMostly, LazyMigrationPolicy)
CONFIGURE_BENCHMARK(Scan, LazyMigrationPolicy)

CONFIGURE_BENCHMARK(UpdateHeavy, EagerMigrationPolicy)
CONFIGURE_BENCHMARK(ReadMostly, EagerMigrationPolicy)
CONFIGURE_BENCHMARK(Scan, EagerMigrationPolicy)

CONFIGURE_BENCHMARK(UpdateHeavy, DramOnlyMigrationPolicy)
CONFIGURE_BENCHMARK(ReadMostly, DramOnlyMigrationPolicy)
CONFIGURE_BENCHMARK(Scan, DramOnlyMigrationPolicy)

CONFIGURE_BENCHMARK(UpdateHeavy, NumaOnlyMigrationPolicy)
CONFIGURE_BENCHMARK(ReadMostly, NumaOnlyMigrationPolicy)
CONFIGURE_BENCHMARK(Scan, NumaOnlyMigrationPolicy)

}  // namespace hyrise