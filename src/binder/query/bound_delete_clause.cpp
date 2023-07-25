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

} // namespace binder
} // namespace kuzu
