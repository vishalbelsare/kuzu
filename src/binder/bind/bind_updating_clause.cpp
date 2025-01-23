#include "binder/binder.h"
#include "binder/expression/expression_util.h"
#include "binder/expression/property_expression.h"
#include "binder/query/query_graph_label_analyzer.h"
#include "binder/query/updating_clause/bound_delete_clause.h"
#include "binder/query/updating_clause/bound_insert_clause.h"
#include "binder/query/updating_clause/bound_merge_clause.h"
#include "binder/query/updating_clause/bound_set_clause.h"
#include "catalog/catalog.h"
#include "catalog/catalog_entry/node_table_catalog_entry.h"
#include "common/assert.h"
#include "common/exception/binder.h"
#include "common/string_format.h"
#include "parser/query/updating_clause/delete_clause.h"
#include "parser/query/updating_clause/insert_clause.h"
#include "parser/query/updating_clause/merge_clause.h"
#include "parser/query/updating_clause/set_clause.h"

using namespace kuzu::common;
using namespace kuzu::parser;
using namespace kuzu::catalog;

namespace kuzu {
namespace binder {

std::unique_ptr<BoundUpdatingClause> Binder::bindUpdatingClause(
    const UpdatingClause& updatingClause) {
    switch (updatingClause.getClauseType()) {
    case ClauseType::INSERT: {
        return bindInsertClause(updatingClause);
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
        KU_UNREACHABLE;
    }
}

static std::unordered_set<std::string> populatePatternsScope(const BinderScope& scope) {
    std::unordered_set<std::string> result;
    for (auto& expression : scope.getExpressions()) {
        if (ExpressionUtil::isNodePattern(*expression) ||
            ExpressionUtil::isRelPattern(*expression)) {
            result.insert(expression->toString());
        } else if (expression->expressionType == ExpressionType::VARIABLE) {
            if (scope.hasNodeReplacement(expression->toString())) {
                result.insert(expression->toString());
            }
        }
    }
    return result;
}

std::unique_ptr<BoundUpdatingClause> Binder::bindInsertClause(
    const UpdatingClause& updatingClause) {
    auto& insertClause = updatingClause.constCast<InsertClause>();
    auto patternsScope = populatePatternsScope(scope);
    // bindGraphPattern will update scope.
    auto boundGraphPattern = bindGraphPattern(insertClause.getPatternElementsRef());
    auto insertInfos = bindInsertInfos(boundGraphPattern.queryGraphCollection, patternsScope);
    return std::make_unique<BoundInsertClause>(std::move(insertInfos));
}

std::unique_ptr<BoundUpdatingClause> Binder::bindMergeClause(const UpdatingClause& updatingClause) {
    auto& mergeClause = updatingClause.constCast<MergeClause>();
    auto patternsScope = populatePatternsScope(scope);
    // bindGraphPattern will update scope.
    auto boundGraphPattern = bindGraphPattern(mergeClause.getPatternElementsRef());
    rewriteMatchPattern(boundGraphPattern);
    auto existenceMark =
        expressionBinder.createVariableExpression(LogicalType::BOOL(), std::string("__existence"));
    auto distinctMark =
        expressionBinder.createVariableExpression(LogicalType::BOOL(), std::string("__distinct"));
    auto createInfos = bindInsertInfos(boundGraphPattern.queryGraphCollection, patternsScope);
    auto boundMergeClause = std::make_unique<BoundMergeClause>(std::move(existenceMark),
        std::move(distinctMark), std::move(boundGraphPattern.queryGraphCollection),
        std::move(boundGraphPattern.where), std::move(createInfos));
    if (mergeClause.hasOnMatchSetItems()) {
        for (auto& [lhs, rhs] : mergeClause.getOnMatchSetItemsRef()) {
            auto setPropertyInfo = bindSetPropertyInfo(lhs.get(), rhs.get());
            boundMergeClause->addOnMatchSetPropertyInfo(std::move(setPropertyInfo));
        }
    }
    if (mergeClause.hasOnCreateSetItems()) {
        for (auto& [lhs, rhs] : mergeClause.getOnCreateSetItemsRef()) {
            auto setPropertyInfo = bindSetPropertyInfo(lhs.get(), rhs.get());
            boundMergeClause->addOnCreateSetPropertyInfo(std::move(setPropertyInfo));
        }
    }
    return boundMergeClause;
}

std::vector<BoundInsertInfo> Binder::bindInsertInfos(QueryGraphCollection& queryGraphCollection,
    const std::unordered_set<std::string>& patternsInScope_) {
    auto patternsInScope = patternsInScope_;
    std::vector<BoundInsertInfo> result;
    auto analyzer = QueryGraphLabelAnalyzer(*clientContext, true /* throwOnViolate */);
    for (auto i = 0u; i < queryGraphCollection.getNumQueryGraphs(); ++i) {
        auto queryGraph = queryGraphCollection.getQueryGraphUnsafe(i);
        // Ensure query graph does not violate declared schema.
        analyzer.pruneLabel(*queryGraph);
        for (auto j = 0u; j < queryGraph->getNumQueryNodes(); ++j) {
            auto node = queryGraph->getQueryNode(j);
            if (node->getVariableName().empty()) { // Always create anonymous node.
                bindInsertNode(node, result);
                continue;
            }
            if (patternsInScope.contains(node->getVariableName())) {
                continue;
            }
            patternsInScope.insert(node->getVariableName());
            bindInsertNode(node, result);
        }
        for (auto j = 0u; j < queryGraph->getNumQueryRels(); ++j) {
            auto rel = queryGraph->getQueryRel(j);
            if (rel->getDirectionType() == RelDirectionType::BOTH) {
                throw BinderException(
                    stringFormat("Create undirected relationship is not supported. Try create 2 "
                                 "directed relationships instead."));
            }
            if (rel->getVariableName().empty()) { // Always create anonymous rel.
                bindInsertRel(rel, result);
                continue;
            }
            if (patternsInScope.contains(rel->getVariableName())) {
                continue;
            }
            patternsInScope.insert(rel->getVariableName());
            bindInsertRel(rel, result);
        }
    }
    if (result.empty()) {
        throw BinderException("Cannot resolve any node or relationship to create.");
    }
    return result;
}

static void validatePrimaryKeyExistence(const NodeTableCatalogEntry* nodeTableEntry,
    const NodeExpression& node, const expression_vector& defaultExprs) {
    auto primaryKeyName = nodeTableEntry->getPrimaryKeyName();
    auto pkeyDefaultExpr = defaultExprs.at(nodeTableEntry->getPrimaryKeyID());
    if (!node.hasPropertyDataExpr(primaryKeyName) &&
        ExpressionUtil::isNullLiteral(*pkeyDefaultExpr)) {
        throw BinderException(stringFormat("Create node {} expects primary key {} as input.",
            node.toString(), primaryKeyName));
    }
}

void Binder::bindInsertNode(std::shared_ptr<NodeExpression> node,
    std::vector<BoundInsertInfo>& infos) {
    if (node->isMultiLabeled()) {
        throw BinderException(
            "Create node " + node->toString() + " with multiple node labels is not supported.");
    }
    auto entry = node->getSingleEntry();
    KU_ASSERT(entry->getTableType() == TableType::NODE);
    auto insertInfo = BoundInsertInfo(TableType::NODE, node);
    for (auto& expr : node->getPropertyExprs()) {
        auto propertyExpr = expr->constPtrCast<PropertyExpression>();
        if (propertyExpr->hasProperty(entry->getTableID())) {
            insertInfo.columnExprs.push_back(expr);
        }
    }
    insertInfo.columnDataExprs =
        bindInsertColumnDataExprs(node->getPropertyDataExprRef(), entry->getProperties());
    auto nodeEntry = entry->ptrCast<NodeTableCatalogEntry>();
    validatePrimaryKeyExistence(nodeEntry, *node, insertInfo.columnDataExprs);
    infos.push_back(std::move(insertInfo));
}

void Binder::bindInsertRel(std::shared_ptr<RelExpression> rel,
    std::vector<BoundInsertInfo>& infos) {
    if (rel->isMultiLabeled() || rel->isBoundByMultiLabeledNode()) {
        throw BinderException(
            "Create rel " + rel->toString() +
            " with multiple rel labels or bound by multiple node labels is not supported.");
    }
    if (ExpressionUtil::isRecursiveRelPattern(*rel)) {
        throw BinderException(stringFormat("Cannot create recursive rel {}.", rel->toString()));
    }
    rel->setEntries(std::vector<TableCatalogEntry*>{rel->getEntries()[0]});
    auto entry = rel->getSingleEntry();
    auto insertInfo = BoundInsertInfo(TableType::REL, rel);
    insertInfo.columnExprs = rel->getPropertyExprs();
    insertInfo.columnDataExprs =
        bindInsertColumnDataExprs(rel->getPropertyDataExprRef(), entry->getProperties());
    infos.push_back(std::move(insertInfo));
}

expression_vector Binder::bindInsertColumnDataExprs(
    const case_insensitive_map_t<std::shared_ptr<Expression>>& propertyDataExprs,
    const std::vector<PropertyDefinition>& propertyDefinitions) {
    expression_vector result;
    for (auto& definition : propertyDefinitions) {
        std::shared_ptr<Expression> rhs;
        if (propertyDataExprs.contains(definition.getName())) {
            rhs = propertyDataExprs.at(definition.getName());
        } else {
            rhs = expressionBinder.bindExpression(*definition.defaultExpr);
        }
        rhs = expressionBinder.implicitCastIfNecessary(rhs, definition.getType());
        result.push_back(std::move(rhs));
    }
    return result;
}

std::unique_ptr<BoundUpdatingClause> Binder::bindSetClause(const UpdatingClause& updatingClause) {
    auto& setClause = updatingClause.constCast<SetClause>();
    auto boundSetClause = std::make_unique<BoundSetClause>();
    for (auto& setItem : setClause.getSetItemsRef()) {
        boundSetClause->addInfo(bindSetPropertyInfo(setItem.first.get(), setItem.second.get()));
    }
    return boundSetClause;
}

BoundSetPropertyInfo Binder::bindSetPropertyInfo(const ParsedExpression* column,
    const ParsedExpression* columnData) {
    auto expr = expressionBinder.bindExpression(*column->getChild(0));
    auto isNode = ExpressionUtil::isNodePattern(*expr);
    auto isRel = ExpressionUtil::isRelPattern(*expr);
    if (!isNode && !isRel) {
        throw BinderException(
            stringFormat("Cannot set expression {} with type {}. Expect node or rel pattern.",
                expr->toString(), ExpressionTypeUtil::toString(expr->expressionType)));
    }
    auto& nodeOrRel = expr->constCast<NodeOrRelExpression>();
    auto boundSetItem = bindSetItem(column, columnData);
    auto boundColumn = boundSetItem.first;
    auto boundColumnData = boundSetItem.second;
    if (isNode) {
        auto info = BoundSetPropertyInfo(TableType::NODE, expr, boundColumn, boundColumnData);
        auto& property = boundSetItem.first->constCast<PropertyExpression>();
        for (auto entry : nodeOrRel.getEntries()) {
            if (property.isPrimaryKey(entry->getTableID())) {
                info.updatePk = true;
            }
        }
        return info;
    }
    return BoundSetPropertyInfo(TableType::REL, expr, boundColumn, boundColumnData);
}

expression_pair Binder::bindSetItem(const ParsedExpression* column,
    const ParsedExpression* columnData) {
    auto boundColumn = expressionBinder.bindExpression(*column);
    auto boundColumnData = expressionBinder.bindExpression(*columnData);
    boundColumnData =
        expressionBinder.implicitCastIfNecessary(boundColumnData, boundColumn->dataType);
    return make_pair(std::move(boundColumn), std::move(boundColumnData));
}

std::unique_ptr<BoundUpdatingClause> Binder::bindDeleteClause(
    const UpdatingClause& updatingClause) {
    auto& deleteClause = updatingClause.constCast<DeleteClause>();
    auto deleteType = deleteClause.getDeleteClauseType();
    auto boundDeleteClause = std::make_unique<BoundDeleteClause>();
    for (auto i = 0u; i < deleteClause.getNumExpressions(); ++i) {
        auto pattern = expressionBinder.bindExpression(*deleteClause.getExpression(i));
        if (ExpressionUtil::isNodePattern(*pattern)) {
            auto deleteNodeInfo = BoundDeleteInfo(deleteType, TableType::NODE, pattern);
            boundDeleteClause->addInfo(std::move(deleteNodeInfo));
        } else if (ExpressionUtil::isRelPattern(*pattern)) {
            // LCOV_EXCL_START
            if (deleteClause.getDeleteClauseType() == DeleteNodeType::DETACH_DELETE) {
                // TODO(Xiyang): Dummy check here. Make sure this is the correct semantic.
                throw BinderException("Detach delete on rel tables is not supported.");
            }
            // LCOV_EXCL_STOP
            auto rel = pattern->constPtrCast<RelExpression>();
            if (rel->getDirectionType() == RelDirectionType::BOTH) {
                throw BinderException("Delete undirected rel is not supported.");
            }
            auto deleteRel = BoundDeleteInfo(deleteType, TableType::REL, pattern);
            boundDeleteClause->addInfo(std::move(deleteRel));
        } else {
            throw BinderException(stringFormat(
                "Cannot delete expression {} with type {}. Expect node or rel pattern.",
                pattern->toString(), ExpressionTypeUtil::toString(pattern->expressionType)));
        }
    }
    return boundDeleteClause;
}

} // namespace binder
} // namespace kuzu
