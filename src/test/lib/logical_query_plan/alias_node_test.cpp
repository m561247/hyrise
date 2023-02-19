#include "base_test.hpp"

#include "expression/lqp_column_expression.hpp"
#include "logical_query_plan/alias_node.hpp"
#include "logical_query_plan/lqp_utils.hpp"
#include "logical_query_plan/mock_node.hpp"
#include "storage/constraints/table_key_constraint.hpp"
#include "utils/data_dependency_test_utils.hpp"

namespace hyrise {

class AliasNodeTest : public BaseTest {
 public:
  void SetUp() override {
    mock_node = MockNode::make(MockNode::ColumnDefinitions{{DataType::Int, "a"}, {DataType::Float, "b"}});
    a = mock_node->get_column("a");
    b = mock_node->get_column("b");

    aliases = {"x", "y"};
    expressions = {b, a};
    alias_node = AliasNode::make(expressions, aliases, mock_node);
  }

  std::vector<std::string> aliases;
  std::vector<std::shared_ptr<AbstractExpression>> expressions;
  std::shared_ptr<MockNode> mock_node;

  std::shared_ptr<AbstractExpression> a, b;
  std::shared_ptr<AliasNode> alias_node;
};

TEST_F(AliasNodeTest, NodeExpressions) {
  ASSERT_EQ(alias_node->node_expressions.size(), 2u);
  EXPECT_EQ(alias_node->node_expressions.at(0), b);
  EXPECT_EQ(alias_node->node_expressions.at(1), a);
}

TEST_F(AliasNodeTest, ShallowEqualsAndCopy) {
  const auto alias_node_copy = alias_node->deep_copy();
  const auto node_mapping = lqp_create_node_mapping(alias_node, alias_node_copy);

  EXPECT_TRUE(alias_node->shallow_equals(*alias_node_copy, node_mapping));
}

TEST_F(AliasNodeTest, HashingAndEqualityCheck) {
  const auto alias_node_copy = alias_node->deep_copy();
  EXPECT_EQ(*alias_node, *alias_node_copy);

  const auto alias_node_other_aliases = AliasNode::make(expressions, std::vector<std::string>{"a", "b"}, mock_node);
  EXPECT_NE(*alias_node, *alias_node_other_aliases);

  const auto other_mock_node =
      MockNode::make(MockNode::ColumnDefinitions{{DataType::Int, "a"}, {DataType::Float, "b"}}, "named");
  const auto expr_a = other_mock_node->get_column("a");
  const auto expr_b = other_mock_node->get_column("b");
  const auto other_expressions = std::vector<std::shared_ptr<AbstractExpression>>{expr_a, expr_b};
  const auto alias_node_other_expressions = AliasNode::make(other_expressions, aliases, mock_node);
  EXPECT_NE(*alias_node, *alias_node_other_expressions);
  const auto alias_node_other_left_input = AliasNode::make(expressions, aliases, other_mock_node);
  EXPECT_NE(*alias_node, *alias_node_other_left_input);

  EXPECT_NE(alias_node->hash(), alias_node_other_expressions->hash());
  EXPECT_EQ(alias_node->hash(), alias_node_other_left_input->hash());
  // alias_node == alias_node_other_left_input is false but the hash codes of these nodes are equal. The reason for this
  // is in the LQPColumnExpressions: Semantically equal LQPColumnExpressions are not equal if they refer to different
  // original_nodes. This allows, e.g., for self-joins. The hash function does not take the actual pointer into account,
  // so the hashes of semantically equal LQPColumnExpressions are equal. The following lines show this fact in detail:
  EXPECT_NE(*a, *expr_a);
  EXPECT_NE(*b, *expr_b);
  EXPECT_EQ(a->hash(), expr_a->hash());
  EXPECT_EQ(b->hash(), expr_b->hash());
}

TEST_F(AliasNodeTest, UniqueColumnCombinationsEmpty) {
  EXPECT_TRUE(mock_node->unique_column_combinations().empty());
  EXPECT_TRUE(alias_node->unique_column_combinations().empty());
}

TEST_F(AliasNodeTest, UniqueColumnCombinationsForwarding) {
  // Add constraints to MockNode.
  const auto key_constraint_a_b = TableKeyConstraint{{ColumnID{0}, ColumnID{1}}, KeyConstraintType::PRIMARY_KEY};
  const auto key_constraint_b = TableKeyConstraint{{ColumnID{1}}, KeyConstraintType::UNIQUE};
  mock_node->set_key_constraints({key_constraint_a_b, key_constraint_b});

  // Basic check.
  const auto& unique_column_combinations = alias_node->unique_column_combinations();
  EXPECT_EQ(unique_column_combinations.size(), 2);
  // In-depth check.
  EXPECT_TRUE(find_ucc_by_key_constraint(key_constraint_a_b, unique_column_combinations));
  EXPECT_TRUE(find_ucc_by_key_constraint(key_constraint_b, unique_column_combinations));
}

TEST_F(AliasNodeTest, ForwardOrderDependencies) {
  EXPECT_TRUE(mock_node->order_dependencies().empty());
  EXPECT_TRUE(alias_node->order_dependencies().empty());

  const auto od = OrderDependency{{a}, {b}};
  const auto order_constraint = TableOrderConstraint{{ColumnID{0}}, {ColumnID{1}}};
  mock_node->set_order_constraints({order_constraint});
  EXPECT_EQ(mock_node->order_dependencies().size(), 1);

  const auto& order_dependencies = alias_node->order_dependencies();
  EXPECT_EQ(order_dependencies.size(), 1);
  EXPECT_TRUE(order_dependencies.contains(od));
}

TEST_F(AliasNodeTest, ForwardInclusionDependencies) {
  EXPECT_TRUE(mock_node->inclusion_dependencies().empty());
  EXPECT_TRUE(alias_node->inclusion_dependencies().empty());

  const auto dummy_table = Table::create_dummy_table({{"a", DataType::Int, false}});
  const auto ind = InclusionDependency{{a}, {ColumnID{0}}, dummy_table};
  const auto foreign_key_constraint = ForeignKeyConstraint{{ColumnID{0}}, {ColumnID{0}}, nullptr, dummy_table};
  mock_node->set_foreign_key_constraints({foreign_key_constraint});
  EXPECT_EQ(mock_node->inclusion_dependencies().size(), 1);

  const auto& inclusion_dependencies = alias_node->inclusion_dependencies();
  EXPECT_EQ(inclusion_dependencies.size(), 1);
  EXPECT_TRUE(inclusion_dependencies.contains(ind));
}

}  // namespace hyrise
