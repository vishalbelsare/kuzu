#include "binder/binder.h"
#include "binder/query/updating_clause/bound_create_clause.h"
#include "binder/query/updating_clause/bound_delete_clause.h"
#include "binder/query/updating_clause/bound_merge_clause.h"
#include "binder/query/updating_clause/bound_set_clause.h"
#include "parser/query/updating_clause/create_clause.h"
#include "parser/query/updating_clause/delete_clause.h"
#include "parser/query/updating_clause/merge_clause.h"
#include "parser/query/updating_clause/set_clause.h"

using namespace kuzu::common;
using namespace kuzu::parser;

namespace kuzu {
namespace binder {

std::unique_ptr<BoundUpdatingClause> Binder::bindUpdatingClause(
    const UpdatingClause& updatingClause) {
    switch (updatingClause.getClauseType()) {
    case ClauseType::CREATE: {
        return bindCreateClause(updatingClause);
    }
    case ClauseType::MERGE: {
        return bindMergeClause(updatingClause);
    }
    case ClauseType::SET: {
        return bindSetClause(updatingClause);
    }
    case ClauseType::DELETE_: {
        return bindDeleteClause(updatingClause);
    }
    default:
        throw NotImplementedException("Binder::bindUpdatingClause");
    }
}

static expression_set populateScope(
    const BinderScope& scope, const std::function<bool(const Expression& expression)>& check) {
    expression_set result;
    for (auto& expression : scope.getExpressions()) {
        if (check(*expression)) {
            result.insert(expression);
        }
    }
    return result;
}

std::unique_ptr<BoundUpdatingClause> Binder::bindCreateClause(
    const UpdatingClause& updatingClause) {
    auto& createClause = (CreateClause&)updatingClause;
    auto nodesScope = populateScope(*scope, ExpressionUtil::isNodeVariable);
    auto relsScope = populateScope(*scope, ExpressionUtil::isRelVariable);
    // bindGraphPattern will update scope.
    auto [queryGraphCollection, propertyCollection] =
        bindGraphPattern(createClause.getPatternElementsRef());
    auto createNodeInfos =
        bindCreateNodeInfos(*queryGraphCollection, *propertyCollection, nodesScope);
    auto createRelInfos = bindCreateRelInfos(*queryGraphCollection, *propertyCollection, relsScope);
    return std::make_unique<BoundCreateClause>(
        std::move(createNodeInfos), std::move(createRelInfos));
}

std::unique_ptr<BoundUpdatingClause> Binder::bindMergeClause(
    const parser::UpdatingClause& updatingClause) {
    auto& mergeClause = (MergeClause&)updatingClause;
    auto nodesScope = populateScope(*scope, ExpressionUtil::isNodeVariable);
    auto relsScope = populateScope(*scope, ExpressionUtil::isRelVariable);
    // bindGraphPattern will update scope.
    auto [queryGraphCollection, propertyCollection] =
        bindGraphPattern(mergeClause.getPatternElementsRef());
    auto createNodeInfos =
        bindCreateNodeInfos(*queryGraphCollection, *propertyCollection, nodesScope);
    auto createRelInfos = bindCreateRelInfos(*queryGraphCollection, *propertyCollection, relsScope);
    auto boundMergeClause = std::make_unique<BoundMergeClause>(
        std::move(queryGraphCollection), std::move(createNodeInfos), std::move(createRelInfos));
    if (mergeClause.hasOnMatchSetItems()) {
        for (auto i = 0u; i < mergeClause.getNumOnMatchSetItems(); ++i) {
            auto setPropertyInfo = bindSetPropertyInfo(mergeClause.getOnMatchSetItem(i));
            boundMergeClause->addOnMatchSetPropertyInfo(std::move(setPropertyInfo));
        }
    }
    if (mergeClause.hasOnCreateSetItems()) {
        for (auto i = 0u; i < mergeClause.getNumOnCreateSetItems(); ++i) {
            auto setPropertyInfo = bindSetPropertyInfo(mergeClause.getOnCreateSetItem(i));
            boundMergeClause->addOnCreateSetPropertyInfo(std::move(setPropertyInfo));
        }
    }
    return boundMergeClause;
}

std::vector<std::unique_ptr<BoundCreateNodeInfo>> Binder::bindCreateNodeInfos(
    const QueryGraphCollection& queryGraphCollection,
    const PropertyKeyValCollection& keyValCollection, const expression_set& nodesScope_) {
    auto nodesScope = nodesScope_;
    std::vector<std::unique_ptr<BoundCreateNodeInfo>> result;
    for (auto i = 0u; i < queryGraphCollection.getNumQueryGraphs(); ++i) {
        auto queryGraph = queryGraphCollection.getQueryGraph(i);
        for (auto j = 0u; j < queryGraph->getNumQueryNodes(); ++j) {
            auto node = queryGraph->getQueryNode(j);
            if (nodesScope.contains(node)) {
                continue;
            }
            nodesScope.insert(node);
            result.push_back(bindCreateNodeInfo(node, keyValCollection));
        }
    }
    return result;
}

std::vector<std::unique_ptr<BoundCreateRelInfo>> Binder::bindCreateRelInfos(
    const QueryGraphCollection& queryGraphCollection,
    const PropertyKeyValCollection& keyValCollection, const expression_set& relsScope_) {
    auto relsScope = relsScope_;
    std::vector<std::unique_ptr<BoundCreateRelInfo>> result;
    for (auto i = 0u; i < queryGraphCollection.getNumQueryGraphs(); ++i) {
        auto queryGraph = queryGraphCollection.getQueryGraph(i);
        for (auto j = 0u; j < queryGraph->getNumQueryRels(); ++j) {
            auto rel = queryGraph->getQueryRel(j);
            if (relsScope.contains(rel)) {
                continue;
            }
            relsScope.insert(rel);
            result.push_back(bindCreateRelInfo(rel, keyValCollection));
        }
    }
    return result;
}

std::unique_ptr<BoundCreateNodeInfo> Binder::bindCreateNodeInfo(
    std::shared_ptr<NodeExpression> node, const PropertyKeyValCollection& collection) {
    if (node->isMultiLabeled()) {
        throw BinderException(
            "Create node " + node->toString() + " with multiple node labels is not supported.");
    }
    auto nodeTableID = node->getSingleTableID();
    auto nodeTableSchema = catalog.getReadOnlyVersion()->getNodeTableSchema(nodeTableID);
    auto primaryKey = nodeTableSchema->getPrimaryKey();
    std::shared_ptr<Expression> primaryKeyExpression;
    std::vector<expression_pair> setItems;
    for (auto& property : catalog.getReadOnlyVersion()->getNodeProperties(nodeTableID)) {
        if (collection.hasKeyVal(node, property.name)) {
            setItems.emplace_back(collection.getKeyVal(node, property.name));
        } else {
            auto propertyExpression =
                expressionBinder.bindNodePropertyExpression(*node, property.name);
            auto nullExpression = expressionBinder.createNullLiteralExpression();
            nullExpression = ExpressionBinder::implicitCastIfNecessary(
                nullExpression, propertyExpression->dataType);
            setItems.emplace_back(std::move(propertyExpression), std::move(nullExpression));
        }
    }
    for (auto& [key, val] : collection.getKeyVals(node)) {
        auto propertyExpression = static_pointer_cast<PropertyExpression>(key);
        if (propertyExpression->getPropertyID(nodeTableID) == primaryKey.propertyID) {
            primaryKeyExpression = val;
        }
    }
    if (nodeTableSchema->getPrimaryKey().dataType.getLogicalTypeID() != LogicalTypeID::SERIAL &&
        primaryKeyExpression == nullptr) {
        throw BinderException("Create node " + node->toString() + " expects primary key " +
                              primaryKey.name + " as input.");
    }
    return std::make_unique<BoundCreateNodeInfo>(
        std::move(node), std::move(primaryKeyExpression), std::move(setItems));
}

std::unique_ptr<BoundCreateRelInfo> Binder::bindCreateRelInfo(
    std::shared_ptr<RelExpression> rel, const PropertyKeyValCollection& collection) {
    if (rel->isMultiLabeled() || rel->isBoundByMultiLabeledNode()) {
        throw BinderException(
            "Create rel " + rel->toString() +
            " with multiple rel labels or bound by multiple node labels is not supported.");
    }
    auto relTableID = rel->getSingleTableID();
    auto catalogContent = catalog.getReadOnlyVersion();
    // CreateRel requires all properties in schema as input. So we rewrite set property to
    // null if user does not specify a property in the query.
    std::vector<expression_pair> setItems;
    for (auto& property : catalogContent->getRelProperties(relTableID)) {
        if (collection.hasKeyVal(rel, property.name)) {
            setItems.push_back(collection.getKeyVal(rel, property.name));
        } else {
            auto propertyExpression =
                expressionBinder.bindRelPropertyExpression(*rel, property.name);
            auto nullExpression = expressionBinder.createNullLiteralExpression();
            nullExpression = ExpressionBinder::implicitCastIfNecessary(
                nullExpression, propertyExpression->dataType);
            setItems.emplace_back(std::move(propertyExpression), std::move(nullExpression));
        }
    }
    return std::make_unique<BoundCreateRelInfo>(std::move(rel), std::move(setItems));
}

std::unique_ptr<BoundUpdatingClause> Binder::bindSetClause(const UpdatingClause& updatingClause) {
    auto& setClause = (SetClause&)updatingClause;
    auto boundSetClause = std::make_unique<BoundSetClause>();
    for (auto i = 0u; i < setClause.getNumSetItems(); ++i) {
        boundSetClause->addSetPropertyInfo(bindSetPropertyInfo(setClause.getSetItem(i)));
    }
    return boundSetClause;
}

static void validateSetNodeProperty(const Expression& expression) {
    auto& node = (const NodeExpression&)expression;
    if (node.isMultiLabeled()) {
        throw BinderException("Set property of node " + node.toString() +
                              " with multiple node labels is not supported.");
    }
}

static void validateSetRelProperty(const Expression& expression) {
    auto& rel = (const RelExpression&)expression;
    if (rel.isMultiLabeled() || rel.isBoundByMultiLabeledNode()) {
        throw BinderException("Set property of rel " + rel.toString() +
                              " with multiple rel labels or bound by multiple node labels "
                              "is not supported.");
    }
}

std::unique_ptr<BoundSetPropertyInfo> Binder::bindSetPropertyInfo(
    std::pair<parser::ParsedExpression*, parser::ParsedExpression*> setItem) {
    auto left = expressionBinder.bindExpression(*setItem.first->getChild(0));
    switch (left->dataType.getLogicalTypeID()) {
    case LogicalTypeID::NODE: {
        validateSetNodeProperty(*left);
        return std::make_unique<BoundSetPropertyInfo>(
            SetPropertyType::NODE, left, bindSetItem(setItem));
    }
    case LogicalTypeID::REL: {
        validateSetRelProperty(*left);
        return std::make_unique<BoundSetPropertyInfo>(
            SetPropertyType::REL, left, bindSetItem(setItem));
    }
    default:
        throw BinderException(
            "Set " + expressionTypeToString(left->expressionType) + " property is supported.");
    }
}

expression_pair Binder::bindSetItem(std::pair<ParsedExpression*, ParsedExpression*> setItem) {
    auto boundLhs = expressionBinder.bindExpression(*setItem.first);
    auto boundRhs = expressionBinder.bindExpression(*setItem.second);
    boundRhs = ExpressionBinder::implicitCastIfNecessary(boundRhs, boundLhs->dataType);
    return make_pair(std::move(boundLhs), std::move(boundRhs));
}

std::unique_ptr<BoundUpdatingClause> Binder::bindDeleteClause(
    const UpdatingClause& updatingClause) {
    auto& deleteClause = (DeleteClause&)updatingClause;
    auto boundDeleteClause = std::make_unique<BoundDeleteClause>();
    for (auto i = 0u; i < deleteClause.getNumExpressions(); ++i) {
        auto nodeOrRel = expressionBinder.bindExpression(*deleteClause.getExpression(i));
        switch (nodeOrRel->dataType.getLogicalTypeID()) {
        case LogicalTypeID::NODE: {
            auto deleteNodeInfo =
                bindDeleteNodeInfo(static_pointer_cast<NodeExpression>(nodeOrRel));
            boundDeleteClause->addDeleteNode(std::move(deleteNodeInfo));
        } break;
        case LogicalTypeID::REL: {
            auto deleteRel = bindDeleteRel(static_pointer_cast<RelExpression>(nodeOrRel));
            boundDeleteClause->addDeleteRel(std::move(deleteRel));
        } break;
        default:
            throw BinderException("Delete " + expressionTypeToString(nodeOrRel->expressionType) +
                                  " is not supported.");
        }
    }
    return boundDeleteClause;
}

std::unique_ptr<BoundDeleteNodeInfo> Binder::bindDeleteNodeInfo(
    const std::shared_ptr<NodeExpression>& node) {
    if (node->isMultiLabeled()) {
        throw BinderException(
            "Delete node " + node->toString() + " with multiple node labels is not supported.");
    }
    auto nodeTableID = node->getSingleTableID();
    auto nodeTableSchema = catalog.getReadOnlyVersion()->getNodeTableSchema(nodeTableID);
    auto primaryKeyExpression =
        expressionBinder.bindNodePropertyExpression(*node, nodeTableSchema->getPrimaryKey().name);
    return std::make_unique<BoundDeleteNodeInfo>(node, primaryKeyExpression);
}

std::shared_ptr<RelExpression> Binder::bindDeleteRel(std::shared_ptr<RelExpression> rel) {
    if (rel->isMultiLabeled() || rel->isBoundByMultiLabeledNode()) {
        throw BinderException(
            "Delete rel " + rel->toString() +
            " with multiple rel labels or bound by multiple node labels is not supported.");
    }
    return rel;
}

} // namespace binder
} // namespace kuzu
