#pragma once

#include <type_traits>

#include "storage/segment_iterables.hpp"

#include "storage/dictionary_segment.hpp"
#include "storage/fixed_string_dictionary_segment.hpp"

#include "storage/vector_compression/resolve_compressed_vector_type.hpp"

namespace opossum {

template <typename T, typename Dictionary>
class DictionarySegmentIterable : public PointAccessibleSegmentIterable<DictionarySegmentIterable<T, Dictionary>> {
 public:
  explicit DictionarySegmentIterable(const DictionarySegment<T>& segment)
      : _segment{segment}, _dictionary(segment.dictionary()) {}

  explicit DictionarySegmentIterable(const FixedStringDictionarySegment<std::string>& segment)
      : _segment{segment}, _dictionary(segment.fixed_string_dictionary()) {}

  template <typename Functor>
  void _on_with_iterators(const Functor& functor) const {
    resolve_compressed_vector_type(*_segment.attribute_vector(), [&](const auto& vector) {
      using ZsIteratorType = decltype(vector.cbegin());

      auto begin = Iterator<ZsIteratorType>{*_dictionary, _segment.null_value_id(), vector.cbegin(), ChunkOffset{0u}};
      auto end = Iterator<ZsIteratorType>{*_dictionary, _segment.null_value_id(), vector.cend(),
                                          static_cast<ChunkOffset>(_segment.size())};
      functor(begin, end);
    });
  }

  template <typename Functor>
  void _on_with_iterators(const PosList& position_filter, const Functor& functor) const {
    resolve_compressed_vector_type(*_segment.attribute_vector(), [&](const auto& vector) {
      auto decoder = vector.create_decoder();
      using ZsDecoderType = std::decay_t<decltype(*decoder)>;

      auto begin = PointAccessIterator<ZsDecoderType>{*_dictionary, _segment.null_value_id(), *decoder,
                                                      position_filter.cbegin(), position_filter.cbegin()};
      auto end = PointAccessIterator<ZsDecoderType>{*_dictionary, _segment.null_value_id(), *decoder,
                                                    position_filter.cbegin(), position_filter.cend()};
      functor(begin, end);
    });
  }

  size_t _on_size() const { return _segment.size(); }

 private:
  const BaseDictionarySegment& _segment;
  std::shared_ptr<const Dictionary> _dictionary;

 private:
  template <typename ZsIteratorType>
  class Iterator : public BaseSegmentIterator<Iterator<ZsIteratorType>, SegmentIteratorValue<T>> {
   public:
    explicit Iterator(const Dictionary& dictionary, const ValueID null_value_id, const ZsIteratorType attribute_it,
                      ChunkOffset chunk_offset)
        : _dictionary{dictionary},
          _null_value_id{null_value_id},
          _attribute_it{attribute_it},
          _chunk_offset{chunk_offset} {}

   private:
    friend class boost::iterator_core_access;  // grants the boost::iterator_facade access to the private interface

    void increment() {
      ++_attribute_it;
      ++_chunk_offset;
    }

    bool equal(const Iterator& other) const { return _attribute_it == other._attribute_it; }

    SegmentIteratorValue<T> dereference() const {
      const auto value_id = static_cast<ValueID>(*_attribute_it);
      const auto is_null = (value_id == _null_value_id);

      if (is_null) return SegmentIteratorValue<T>{T{}, true, _chunk_offset};

      if constexpr (std::is_same_v<Dictionary, FixedStringVector>) {
        return SegmentIteratorValue<T>{_dictionary.get_string_at(value_id), false, _chunk_offset};
      } else {
        return SegmentIteratorValue<T>{_dictionary[value_id], false, _chunk_offset};
      }
    }

   private:
    const Dictionary& _dictionary;
    const ValueID _null_value_id;
    ZsIteratorType _attribute_it;
    ChunkOffset _chunk_offset;
  };

  template <typename ZsDecoderType>
  class PointAccessIterator
      : public BasePointAccessSegmentIterator<PointAccessIterator<ZsDecoderType>, SegmentIteratorValue<T>> {
   public:
    PointAccessIterator(const Dictionary& dictionary, const ValueID null_value_id, ZsDecoderType& attribute_decoder,
                        const PosList::const_iterator position_filter_begin, PosList::const_iterator position_filter_it)
        : BasePointAccessSegmentIterator<PointAccessIterator<ZsDecoderType>,
                                         SegmentIteratorValue<T>>{std::move(position_filter_begin),
                                                                  std::move(position_filter_it)},
          _dictionary{dictionary},
          _null_value_id{null_value_id},
          _attribute_decoder{attribute_decoder} {}

   private:
    friend class boost::iterator_core_access;  // grants the boost::iterator_facade access to the private interface

    SegmentIteratorValue<T> dereference() const {
      const auto& chunk_offsets = this->chunk_offsets();

      const auto value_id = _attribute_decoder.get(chunk_offsets.offset_in_referenced_chunk);
      const auto is_null = (value_id == _null_value_id);

      if (is_null) return SegmentIteratorValue<T>{T{}, true, chunk_offsets.offset_in_poslist};

      if constexpr (std::is_same_v<Dictionary, FixedStringVector>) {
        return SegmentIteratorValue<T>{_dictionary.get_string_at(value_id), false, chunk_offsets.offset_in_poslist};
      } else {
        return SegmentIteratorValue<T>{_dictionary[value_id], false, chunk_offsets.offset_in_poslist};
      }
    }

   private:
    const Dictionary& _dictionary;
    const ValueID _null_value_id;
    ZsDecoderType& _attribute_decoder;
  };
};

}  // namespace opossum
