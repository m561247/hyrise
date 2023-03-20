#pragma once

#include <boost/unordered/unordered_flat_map.hpp>
#include <memory>
#include <tuple>
#include "storage/buffer/frame.hpp"
#include "storage/buffer/ssd_region.hpp"
#include "storage/buffer/types.hpp"
#include "storage/buffer/volatile_region.hpp"

namespace hyrise {

template <typename PointedType>
class BufferManagedPtr;

// Eviction Queue
struct EvictionItem {
  Frame* frame;
  uint64_t timestamp;
};

using EvictionQueue = tbb::concurrent_queue<EvictionItem>;

/**
 * @brief 
 * 
 * HYRISE_BUFFER_BLOCK_PATH="/dev/nvme3n1"
 * HYRISE_BUFFER_SCRATCH_PATH="/scratch/niklas.riekenbrauck"
 * HYRISE_BUFFER_MANAGER_PATH=$HYRISE_BUFFER_BLOCK_PATH
 * 
 * TODO: Ensure concurrent access via atomics
 */
class BufferManager {
 public:
  /**
   * Metrics are storing metric data that happens during allocation and access of the buffer manager.
   * TODO: Make values atomic
  */
  struct Metrics {
    // Tracks all allocation that are happening on the buffer manager through the BufferPoolAllocator
    // std::vector<std::size_t> allocations_in_bytes{};

    // The maximum amount of bytes being allocated with with subsequent calls of alloc and dealloc
    std::size_t max_bytes_used;

    // The current amount of bytes being allocated
    std::size_t current_bytes_used;

    // The total number of bytes being allocates
    std::size_t total_allocated_bytes;

    // The total number of bytes that is unused when allocating memory on a page. Can be used to calculate internal fragmentation.
    std::size_t total_unused_bytes;

    // The number of allocation
    std::size_t num_allocs;

    // Tracks the number of hits in the page_table
    std::size_t page_table_hits = 0;

    // Tracks the number of hits in the page_table
    std::size_t page_table_misses = 0;

    // Tracks the number of bytes written to SSD
    std::size_t total_bytes_written = 0;

    // Tracks the number of bytes read from SSD
    std::size_t total_bytes_read = 0;

    // TODO: Number of pages used, fragmentation rate
  };

  BufferManager();
  BufferManager(const size_t num_bytes, std::filesystem::path path);

  /**
   * @brief Get the page object
   * 
   * @param page_id 
   * @return std::unique_ptr<Page> 
   */
  std::byte* get_page(const PageID page_id);

  /**
   * @brief Pin a page marks a page unavailable for replacement. It needs to be unpinned before it can be replaced.
   * 
   * @param page_id 
   */
  void pin_page(const PageID page_id);

  /**
   * @brief Unpinning a page marks a page available for replacement. This acts as a soft-release without flushing
   * the page back to disk. Calls callback if the pin count is redced to zero.
   * 
   * @param page_id 
   */
  void unpin_page(const PageID page_id, const bool dirty = false);

  uint32_t get_pin_count(const PageID page_id);

  bool is_dirty(const PageID page_id);

  /**
   * @brief Get the page id and offset from ptr object. PageID is on its max 
   * if the page there was no page found
   * 
   * @param ptr 
   * @return std::pair<PageID, PageOffset> 
   */
  std::tuple<PageID, PageSizeType, std::ptrdiff_t> unswizzle(const void* ptr);

  /**
   * Allocates pages to fullfil allocation request of the given bytes and alignment 
  */
  BufferManagedPtr<void> allocate(std::size_t bytes, std::size_t align = alignof(std::max_align_t));

  /**
   * Deallocates a pointer and frees the pages.
  */
  void deallocate(BufferManagedPtr<void> ptr, std::size_t bytes, std::size_t align = alignof(std::max_align_t));

  /**
   * @brief Helper function to get the BufferManager singleton. This avoids issues with circular dependencies as the implementation in the .cpp file.
   * 
   * @return BufferManager& 
   */
  static BufferManager& get_global_buffer_manager();

  /**
   * @brief Returns a metrics struture holding information about allocations, page table hits etc. of the current buffer manager instance. Can be reset by
   * assigning a new instance
  */
  Metrics& metrics();

  BufferManager& operator=(BufferManager&& other);

  /**
   * Reset all data in the internal data structures
  */
  void soft_reset();

 protected:
  friend class Hyrise;

 private:
  Frame* allocate_frame(const PageSizeType size_type);
  Frame* get_frame(const PageID page_id);

  Frame* find_in_page_table(const PageID page_id);

  Frame* new_page(const PageSizeType size_type);
  void remove_page(const PageID page_id);

  void read_page(Frame* frame);
  void write_page(const Frame* frame);

  std::atomic_uint64_t _num_pages;

  const size_t _total_bytes;
  std::atomic_uint64_t _used_bytes;

  // Memory Region for pages on SSD
  std::unique_ptr<SSDRegion> _ssd_region;

  // Page Table that contains frames (= pages) which are currently in the buffer pool
  boost::unordered_flat_map<PageID, Frame*> _page_table;

  std::array<VolatileRegion, NUM_PAGE_SIZE_TYPES> _buffer_pools;

  std::mutex _page_table_mutex;

  EvictionQueue _eviction_queue;

  // Metrics of buffer manager
  Metrics _metrics{};
};

}  // namespace hyrise