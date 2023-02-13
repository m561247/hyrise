#include <memory>
#include <vector>

#include <boost/container/vector.hpp>
#include "benchmark/benchmark.h"
#include "storage/buffer/buffer_manager.hpp"
#include "storage/buffer/buffer_pool_allocator.hpp"
#include "storage/buffer/utils.hpp"

namespace hyrise {

static void BM_allocate_pages_buffer_pool_allocator_empty(benchmark::State& state) {
  auto ssd_region = std::make_unique<SSDRegion>(ssd_region_path() / "pool_allocator_benchmark.data");  // TODO: Path
  auto volatile_region = std::make_unique<VolatileRegion>(1 << 20);
  auto buffer_manager = BufferManager(std::move(volatile_region), std::move(ssd_region));
  auto allocator = BufferPoolAllocator<int>(&buffer_manager);

  auto memory_manager = BufferManagerBenchmarkMemoryManager::create_and_register(&buffer_manager);

  auto allocation_count = static_cast<size_t>(state.range(0));
  const auto vector_size = Page32KiB::size() / sizeof(int);
  for (auto _ : state) {
    state.PauseTiming();
    // TODO: Perform a reset here
    state.ResumeTiming();

    for (auto index = size_t{0}; index < allocation_count; index++) {
      auto array = boost::container::vector<int, BufferPoolAllocator<int>>{vector_size, allocator};
      benchmark::DoNotOptimize(array.size());
    }
  }
}

// static void BM_allocate_pages_buffer_pool_allocator_full(benchmark::State& state) {}

static void BM_allocate_pages_std_allocator(benchmark::State& state) {
  auto allocation_count = static_cast<size_t>(state.range(0));
  const auto vector_size = Page32KiB::size() / sizeof(int);
  for (auto _ : state) {
    state.PauseTiming();
    auto allocator = std::allocator<int>();
    state.ResumeTiming();

    for (auto index = size_t{0}; index < allocation_count; index++) {
      auto array = boost::container::vector<int, std::allocator<int>>{vector_size, allocator};
      benchmark::DoNotOptimize(array.size());
    }
  }
}

BENCHMARK(BM_allocate_pages_buffer_pool_allocator_empty)
    ->Name("Multiple allocations of page-sized vector with BufferPoolAllocator")
    ->Range(8, 8 << 9);
BENCHMARK(BM_allocate_pages_std_allocator)
    ->Name("Multiple allocations of page-sized vector with std::allocator")
    ->Range(8, 8 << 9);
}  // namespace hyrise