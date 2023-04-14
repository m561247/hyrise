#include <memory>

#include "base_test.hpp"

#include "logical_query_plan/intersect_node.hpp"
#include "logical_query_plan/lqp_utils.hpp"
#include "logical_query_plan/mock_node.hpp"

namespace hyrise {

class IntersectNodeTest : public BaseTest {
 protected:
  void SetUp() override {
    _mock_node1 = MockNode::make(
        MockNode::ColumnDefinitions{{DataType::Int, "a"}, {DataType::Int, "b"}, {DataType::Int, "c"}}, "t_a");
    _mock_node2 = MockNode::make(MockNode::ColumnDefinitions{{DataType::Int, "u"}, {DataType::Int, "v"}}, "t_b");
    _mock_node3 = MockNode::make(MockNode::ColumnDefinitions{{DataType::Int, "x"}}, "t_v");

    _a = _mock_node1->get_column("a");
    _b = _mock_node1->get_column("b");
    _c = _mock_node1->get_column("c");

    _intersect_node = IntersectNode::make(SetOperationMode::Positions);
    _intersect_node->set_left_input(_mock_node1);
    _intersect_node->set_right_input(_mock_node1);
  }

  std::shared_ptr<MockNode> _mock_node1, _mock_node2, _mock_node3;
  std::shared_ptr<IntersectNode> _intersect_node;
  std::shared_ptr<LQPColumnExpression> _a, _b, _c;
};

TEST_F(IntersectNodeTest, Description) {
  EXPECT_EQ(_intersect_node->description(), "[IntersectNode] Mode: Positions");
}

TEST_F(IntersectNodeTest, OutputColumnExpressions) {
  EXPECT_TRUE(_intersect_node->output_expressions() == _mock_node1->output_expressions());
}

TEST_F(IntersectNodeTest, HashingAndEqualityCheck) {
  auto same_intersect_node = IntersectNode::make(SetOperationMode::Positions, _mock_node1, _mock_node1);
  auto different_intersect_node = IntersectNode::make(SetOperationMode::All, _mock_node1, _mock_node1);
  auto different_intersect_node_1 = IntersectNode::make(SetOperationMode::Positions, _mock_node1, _mock_node2);
  auto different_intersect_node_2 = IntersectNode::make(SetOperationMode::Positions, _mock_node2, _mock_node1);
  auto different_intersect_node_3 = IntersectNode::make(SetOperationMode::Positions, _mock_node2, _mock_node2);

  EXPECT_EQ(*_intersect_node, *same_intersect_node);
  EXPECT_NE(*_intersect_node, *different_intersect_node);
  EXPECT_NE(*_intersect_node, *different_intersect_node_1);
  EXPECT_NE(*_intersect_node, *different_intersect_node_2);
  EXPECT_NE(*_intersect_node, *different_intersect_node_3);
  EXPECT_NE(*_intersect_node, *IntersectNode::make(SetOperationMode::Positions));
  EXPECT_NE(*_intersect_node, *IntersectNode::make(SetOperationMode::All));

  EXPECT_EQ(_intersect_node->hash(), same_intersect_node->hash());
  EXPECT_NE(_intersect_node->hash(), different_intersect_node->hash());
  EXPECT_NE(_intersect_node->hash(), different_intersect_node_1->hash());
  EXPECT_NE(_intersect_node->hash(), different_intersect_node_2->hash());
  EXPECT_NE(_intersect_node->hash(), different_intersect_node_3->hash());
}

TEST_F(IntersectNodeTest, Copy) {
  EXPECT_EQ(*_intersect_node->deep_copy(), *_intersect_node);
}

TEST_F(IntersectNodeTest, NodeExpressions) {
  ASSERT_EQ(_intersect_node->node_expressions.size(), 0u);
}

TEST_F(IntersectNodeTest, ForwardUniqueColumnCombinations) {
  EXPECT_TRUE(_mock_node1->unique_column_combinations().empty());
  EXPECT_TRUE(_intersect_node->unique_column_combinations().empty());

  const auto key_constraint_a = TableKeyConstraint{{_a->original_column_id}, KeyConstraintType::UNIQUE};
  _mock_node1->set_key_constraints({key_constraint_a});
  EXPECT_EQ(_mock_node1->unique_column_combinations().size(), 1);

  const auto& unique_column_combinations = _intersect_node->unique_column_combinations();
  EXPECT_EQ(unique_column_combinations.size(), 1);
  EXPECT_TRUE(unique_column_combinations.contains({UniqueColumnCombination{{_a}}}));

  if constexpr (HYRISE_DEBUG) {
    _intersect_node->set_right_input(_mock_node2);
    EXPECT_THROW(_intersect_node->unique_column_combinations(), std::logic_error);
  }
}

TEST_F(IntersectNodeTest, ForwardOrderDependencies) {
  EXPECT_TRUE(_mock_node1->order_dependencies().empty());
  EXPECT_TRUE(_intersect_node->order_dependencies().empty());

  const auto od_a_to_b = OrderDependency{{_a}, {_b}};
  const auto od_a_to_c = OrderDependency{{_a}, {_c}};
  const auto order_constraint_a_to_b = TableOrderConstraint{{ColumnID{0}}, {ColumnID{1}}};
  const auto order_constraint_a_to_c = TableOrderConstraint{{ColumnID{0}}, {ColumnID{2}}};
  _mock_node1->set_order_constraints({order_constraint_a_to_b, order_constraint_a_to_c});
  EXPECT_EQ(_mock_node1->order_dependencies().size(), 2);

  const auto& order_dependencies = _intersect_node->order_dependencies();
  EXPECT_EQ(order_dependencies.size(), 2);
  EXPECT_TRUE(order_dependencies.contains(od_a_to_b));
  EXPECT_TRUE(order_dependencies.contains(od_a_to_c));

  if constexpr (HYRISE_DEBUG) {
    _intersect_node->set_right_input(_mock_node2);
    EXPECT_THROW(_intersect_node->order_dependencies(), std::logic_error);
  }
}

TEST_F(IntersectNodeTest, NoInclusionDependencies) {
  EXPECT_TRUE(_mock_node1->inclusion_dependencies().empty());
  EXPECT_TRUE(_intersect_node->inclusion_dependencies().empty());

  const auto dummy_table = Table::create_dummy_table({{"a", DataType::Int, false}});
  const auto foreign_key_constraint = ForeignKeyConstraint{{ColumnID{0}}, dummy_table, {ColumnID{0}}, nullptr};
  _mock_node1->set_foreign_key_constraints({foreign_key_constraint});
  EXPECT_EQ(_mock_node1->inclusion_dependencies().size(), 1);

  EXPECT_TRUE(_intersect_node->inclusion_dependencies().empty());
}

}  // namespace hyrise
