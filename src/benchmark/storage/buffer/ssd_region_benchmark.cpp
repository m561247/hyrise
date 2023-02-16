#include <algorithm>
#include <boost/align/aligned_allocator.hpp>
#include <filesystem>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

#include "benchmark/benchmark.h"
#include "micro_benchmark_utils.hpp"
#include "operators/table_wrapper.hpp"
#include "storage/buffer/page.hpp"
#include "storage/buffer/ssd_region.hpp"
#include "storage/buffer/utils.hpp"

namespace hyrise {

// TODO: Test file vs block
static void BM_SSDRegionReadPagesSingle(benchmark::State& state) {
  // micro_benchmark_clear_cache();

  auto ssd_region = SSDRegion("/dev/nvme3n1");
  auto outputPage = Page32KiB();
  const auto num_pages = state.range(0);
  for (auto _ : state) {
    for (auto page_id = PageID{0}; page_id < num_pages; page_id++) {
      ssd_region.read_page(PageID{0}, outputPage);
    }
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(num_pages) * int64_t(Page32KiB::size()));
  state.SetItemsProcessed(int64_t(state.iterations()) * int64_t(num_pages));
}

static void BM_SSDRegionReadPagesSerial(benchmark::State& state) {
  // micro_benchmark_clear_cache();

  auto ssd_region = SSDRegion("/dev/nvme3n1");
  const auto num_pages = state.range(0);
  std::vector<Page32KiB, boost::alignment::aligned_allocator<Page32KiB>> pages(num_pages);

  for (auto _ : state) {
    for (auto page_id = PageID{0}; page_id < num_pages; page_id++) {
      ssd_region.read_page(page_id, pages[page_id]);
    }
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(num_pages) * int64_t(Page32KiB::size()));
  state.SetItemsProcessed(int64_t(state.iterations()) * int64_t(num_pages));
}

static void BM_SSDRegionReadPagesRandom(benchmark::State& state) {
  // micro_benchmark_clear_cache();

  auto ssd_region = SSDRegion("/dev/nvme3n1");
  const auto num_pages = state.range(0);
  std::vector<Page32KiB, boost::alignment::aligned_allocator<Page32KiB>> pages(num_pages);

  std::vector<PageID> random_page_ids(num_pages);
  std::iota(std::begin(random_page_ids), std::end(random_page_ids), 0);
  std::default_random_engine random_engine(100);  // TODO: Seed
  std::shuffle(random_page_ids.begin(), random_page_ids.end(), random_engine);

  for (auto _ : state) {
    for (auto read_index = int64_t{0}; read_index < num_pages; read_index++) {
      ssd_region.read_page(random_page_ids[read_index], pages[read_index]);
    }
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(num_pages) * int64_t(Page32KiB::size()));
  state.SetItemsProcessed(int64_t(state.iterations()) * int64_t(num_pages));
}

static void BM_SSDRegionWritePagesSingle(benchmark::State& state) {
  // micro_benchmark_clear_cache();

  auto ssd_region = SSDRegion("/dev/nvme3n1");
  auto outputPage = Page32KiB();
  const auto num_pages = state.range(0);
  for (auto _ : state) {
    for (auto page_id = PageID{0}; page_id < num_pages; page_id++) {
      ssd_region.write_page(PageID{0}, outputPage);
    }
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(num_pages) * int64_t(Page32KiB::size()));
  state.SetItemsProcessed(int64_t(state.iterations()) * int64_t(num_pages));
}

static void BM_SSDRegionWritePagesSerial(benchmark::State& state) {
  // micro_benchmark_clear_cache();

  auto ssd_region = SSDRegion("/dev/nvme3n1");
  const auto num_pages = state.range(0);
  std::vector<Page32KiB, boost::alignment::aligned_allocator<Page32KiB>> pages(num_pages);
  for (auto _ : state) {
    for (auto page_id = PageID{0}; page_id < num_pages; page_id++) {
      ssd_region.write_page(page_id, pages[page_id]);
      benchmark::DoNotOptimize(pages.size());
    }
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(num_pages) * int64_t(Page32KiB::size()));
  state.SetItemsProcessed(int64_t(state.iterations()) * int64_t(num_pages));
}

BENCHMARK(BM_SSDRegionReadPagesSingle)->RangeMultiplier(2)->Range(2 << 8, 8 << 10);
BENCHMARK(BM_SSDRegionReadPagesSerial)->RangeMultiplier(2)->Range(2 << 8, 8 << 10);
BENCHMARK(BM_SSDRegionReadPagesRandom)->RangeMultiplier(2)->Range(2 << 8, 8 << 10);
BENCHMARK(BM_SSDRegionWritePagesSingle)->RangeMultiplier(2)->Range(2 << 8, 8 << 10);
BENCHMARK(BM_SSDRegionWritePagesSerial)->RangeMultiplier(2)->Range(2 << 8, 8 << 10);

}  // namespace hyrise