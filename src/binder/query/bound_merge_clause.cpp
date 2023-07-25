#include "binder/query/updating_clause/bound_merge_clause.h"

namespace kuzu {
namespace binder {

BoundMergeClause::BoundMergeClause(const BoundMergeClause& other)
    : BoundUpdatingClause{common::ClauseType::MERGE} {
    queryGraphCollection = other.queryGraphCollection->copy();
    for (auto& createNodeInfo : other.createNodeInfos) {
        createNodeInfos.push_back(createNodeInfo->copy());
    }
    for (auto& createRelInfo : other.createRelInfos) {
        createRelInfos.push_back(createRelInfo->copy());
    }
    for (auto& setPropertyInfo : other.onMatchSetPropertyInfos) {
        onMatchSetPropertyInfos.push_back(setPropertyInfo->copy());
    }
    for (auto& setPropertyInfo : other.onCreateSetPropertyInfos) {
        onCreateSetPropertyInfos.push_back(setPropertyInfo->copy());
    }
}

} // namespace binder
} // namespace kuzu
