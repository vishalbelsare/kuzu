#include "binder/query/updating_clause/bound_create_clause.h"

namespace kuzu {
namespace binder {

BoundCreateClause::BoundCreateClause(const BoundCreateClause& other)
    : BoundUpdatingClause{common::ClauseType::CREATE} {
    for (auto& createNodeInfo : other.createNodeInfos) {
        createNodeInfos.push_back(createNodeInfo->copy());
    }
    for (auto& createRelInfo : createRelInfos) {
        createRelInfos.push_back(createRelInfo->copy());
    }
}

std::vector<expression_pair> BoundCreateClause::getAllSetItems() const {
    std::vector<expression_pair> result;
    for (auto& createNodeInfo : createNodeInfos) {
        for (auto& setItem : createNodeInfo->setItems) {
            result.push_back(setItem);
        }
    }
    for (auto& createRelInfo : createRelInfos) {
        for (auto& setItem : createRelInfo->setItems) {
            result.push_back(setItem);
        }
    }
    return result;
}

} // namespace binder
} // namespace kuzu
