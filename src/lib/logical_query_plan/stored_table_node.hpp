#pragma once

#include <optional>
#include <vector>

#include "abstract_lqp_node.hpp"
#include "expression/abstract_expression.hpp"
#include "expression/lqp_column_expression.hpp"
#include "storage/index/index_statistics.hpp"

namespace hyrise {

class LQPColumnExpression;
class TableStatistics;

/**
 * Represents a Table from the StorageManager in an LQP
 *
 * Holds Column and Chunk pruning information.
 */
class StoredTableNode : public EnableMakeForLQPNode<StoredTableNode>, public AbstractLQPNode {
 public:
  explicit StoredTableNode(const std::string& init_table_name);

  std::shared_ptr<LQPColumnExpression> get_column(const std::string& name) const;

  /**
   * @defgroup ColumnIDs and ChunkIDs to be pruned from the stored Table.
   * Both vectors need to be sorted and must no contain duplicates when passed to `set_pruned_{chunk/column}_ids()`
   *
   * @{
   */
  void set_pruned_chunk_ids(const std::vector<ChunkID>& pruned_chunk_ids);
  const std::vector<ChunkID>& pruned_chunk_ids() const;

  void set_pruned_column_ids(const std::vector<ColumnID>& pruned_column_ids);
  const std::vector<ColumnID>& pruned_column_ids() const;
  /** @} */

  std::vector<IndexStatistics> indexes_statistics() const;

  std::string description(const DescriptionMode mode = DescriptionMode::Short) const override;
  std::vector<std::shared_ptr<AbstractExpression>> output_expressions() const override;
  bool is_column_nullable(const ColumnID column_id) const override;

  // Generates unique column combinations from a table's key constraints. Drops UCCs that include pruned columns.
  UniqueColumnCombinations unique_column_combinations() const override;

  OrderDependencies order_dependencies() const override;

  std::shared_ptr<InclusionDependencies> inclusion_dependencies() const override;

  const std::string table_name;

  // By default, the StoredTableNode takes its statistics from the table. This field can be used to overwrite these
  // statistics if they have changed from the original table, e.g., as the result of chunk pruning.
  std::shared_ptr<TableStatistics> table_statistics;

 protected:
  size_t _on_shallow_hash() const override;
  std::shared_ptr<AbstractLQPNode> _on_shallow_copy(LQPNodeMapping& node_mapping) const override;
  bool _on_shallow_equals(const AbstractLQPNode& rhs, const LQPNodeMapping& node_mapping) const override;
  std::shared_ptr<const StoredTableNode> _get_or_find_referenced_node(const std::string& table_name) const;

 private:
  mutable std::optional<std::vector<std::shared_ptr<AbstractExpression>>> _output_expressions;
  std::vector<ChunkID> _pruned_chunk_ids;
  std::vector<ColumnID> _pruned_column_ids;
  mutable std::unordered_map<std::string, std::weak_ptr<const StoredTableNode>> _ind_stored_table_node_cache;
};

}  // namespace hyrise
