#include "binder/query/updating_clause/bound_delete_clause.h"

namespace kuzu {
namespace binder {

BoundDeleteClause::BoundDeleteClause(const BoundDeleteClause& other)
    : BoundUpdatingClause{common::ClauseType::DELETE_} {
    for (auto& deleteNodeInfo : other.deleteNodeInfos) {
        deleteNodeInfos.push_back(deleteNodeInfo->copy());
    }
    for (auto& deleteRel : other.deleteRels) {
        deleteRels.push_back(deleteRel);
    }
}

std::vector<BoundDeleteNodeInfo*> BoundDeleteClause::getNodeInfos() const {
    std::vector<BoundDeleteNodeInfo*> result;
    result.reserve(deleteNodeInfos.size());
    for (auto& info : deleteNodeInfos) {
        result.push_back(info.get());
    }
    return result;
}

} // namespace binder
} // namespace kuzu
