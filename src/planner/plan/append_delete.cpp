#include "binder/query/updating_clause/bound_delete_clause.h"
#include "planner/logical_plan/logical_operator/logical_delete.h"
#include "planner/query_planner.h"

namespace kuzu {
namespace planner {

void QueryPlanner::appendDeleteNode(
    const std::vector<binder::BoundDeleteNodeInfo*>& infos, LogicalPlan& plan) {
    std::vector<std::shared_ptr<NodeExpression>> nodes;
    expression_vector primaryKeys;
    for (auto& info : infos) {
        nodes.push_back(info->node);
        primaryKeys.push_back(info->primaryKey);
    }
    auto deleteNode = std::make_shared<LogicalDeleteNode>(
        std::move(nodes), std::move(primaryKeys), plan.getLastOperator());
    deleteNode->computeFactorizedSchema();
    plan.setLastOperator(std::move(deleteNode));
}

void QueryPlanner::appendDeleteRel(
    const std::vector<std::shared_ptr<binder::RelExpression>>& deleteRels, LogicalPlan& plan) {
    auto deleteRel = std::make_shared<LogicalDeleteRel>(deleteRels, plan.getLastOperator());
    for (auto i = 0u; i < deleteRel->getNumRels(); ++i) {
        appendFlattens(deleteRel->getGroupsPosToFlatten(i), plan);
        deleteRel->setChild(0, plan.getLastOperator());
    }
    deleteRel->computeFactorizedSchema();
    plan.setLastOperator(std::move(deleteRel));
}

} // namespace planner
} // namespace kuzu
