#pragma once

#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <memory>

namespace opossum {

// We need a wrapper around std::experimental::pmr::polymorphic_allocator because tbb::concurrent_vector still relies on
// rebind<U>::other (which is deprecated in C++17)
template <class T>
class PolymorphicAllocator : public boost::container::pmr::polymorphic_allocator<T> {
 public:
  template <class U>
  struct rebind {
    typedef PolymorphicAllocator<U> other;
  };

  PolymorphicAllocator() {}
  template <class U>
  PolymorphicAllocator(const boost::container::pmr::polymorphic_allocator<U>& alloc)  // NOLINT(runtime/explicit)
      : boost::container::pmr::polymorphic_allocator<T>(alloc) {}
  template <class U>
  PolymorphicAllocator(const PolymorphicAllocator<U>& other) : boost::container::pmr::polymorphic_allocator<T>(other) {}
  PolymorphicAllocator(boost::container::pmr::memory_resource* m_resource)  // NOLINT(runtime/explicit)
      : boost::container::pmr::polymorphic_allocator<T>(m_resource) {}
};

}  // namespace opossum
