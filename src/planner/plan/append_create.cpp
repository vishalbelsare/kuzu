#include "binder/query/updating_clause/bound_create_clause.h"
#include "binder/query/updating_clause/bound_set_clause.h"
#include "planner/logical_plan/logical_operator/logical_create.h"
#include "planner/query_planner.h"

using namespace kuzu::binder;

namespace kuzu {
namespace planner {

void QueryPlanner::appendCreateNode(
    const std::vector<binder::BoundCreateNodeInfo*>& createInfos, LogicalPlan& plan) {
    std::vector<std::shared_ptr<NodeExpression>> nodes;
    expression_vector primaryKeys;
    std::vector<std::unique_ptr<BoundSetPropertyInfo>> setInfos;
    for (auto& createInfo : createInfos) {
        nodes.push_back(createInfo->node);
        primaryKeys.push_back(createInfo->primaryKey);
        for (auto& setItem : createInfo->setItems) {
            setInfos.push_back(std::make_unique<BoundSetPropertyInfo>(
                SetPropertyType::NODE, createInfo->node, setItem));
        }
    }
    auto createNode = std::make_shared<LogicalCreateNode>(
        std::move(nodes), std::move(primaryKeys), plan.getLastOperator());
    appendFlattens(createNode->getGroupsPosToFlatten(), plan);
    createNode->setChild(0, plan.getLastOperator());
    createNode->computeFactorizedSchema();
    plan.setLastOperator(createNode);
    std::vector<BoundSetPropertyInfo*> setInfoPtrs;
    for (auto& setInfo : setInfos) {
        setInfoPtrs.push_back(setInfo.get());
    }
    appendSetNodeProperty(setInfoPtrs, plan);
}

void QueryPlanner::appendCreateRel(
    const std::vector<binder::BoundCreateRelInfo*>& createInfos, LogicalPlan& plan) {
    std::vector<std::shared_ptr<RelExpression>> rels;
    std::vector<std::vector<expression_pair>> setItemsPerRel;
    for (auto& info : createInfos) {
        rels.push_back(info->rel);
        setItemsPerRel.push_back(info->setItems);
    }
    auto createRel = std::make_shared<LogicalCreateRel>(
        std::move(rels), std::move(setItemsPerRel), plan.getLastOperator());
    appendFlattens(createRel->getGroupsPosToFlatten(), plan);
    createRel->setChild(0, plan.getLastOperator());
    createRel->computeFactorizedSchema();
    plan.setLastOperator(createRel);
}

} // namespace planner
} // namespace kuzu
