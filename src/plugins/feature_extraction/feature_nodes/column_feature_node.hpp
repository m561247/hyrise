#pragma once

#include "abstract_feature_node.hpp"
#include "expression/lqp_column_expression.hpp"
#include "storage/table.hpp"

namespace opossum {

class ColumnFeatureNode : public AbstractFeatureNode {
 public:
  ColumnFeatureNode(const std::shared_ptr<AbstractFeatureNode>& input_node, const ColumnID column_id);

  static std::shared_ptr<ColumnFeatureNode> from_column_expression(
      const std::shared_ptr<AbstractFeatureNode>& operator_node, const std::shared_ptr<AbstractExpression>& expression);

  const std::vector<std::string>& feature_headers() const final;

  static const std::vector<std::string>& headers();

  ColumnID column_id() const;

 protected:
  std::shared_ptr<FeatureVector> _on_to_feature_vector() const final;
  size_t _on_shallow_hash() const final;

  ColumnID _column_id;
  DataType _data_type;

  ChunkID _chunk_count = ChunkID{0};
  uint64_t _value_segments = 0;
  uint64_t _dictionary_segments = 0;
  uint64_t _fixed_string_dictionary_segments = 0;
  uint64_t _for_segments = 0;
  uint64_t _run_length_segments = 0;
  uint64_t _lz4_segments = 0;
  bool _nullable = false;
  uint64_t _sorted_segments = 0;
  bool _references = false;
};

}  // namespace opossum