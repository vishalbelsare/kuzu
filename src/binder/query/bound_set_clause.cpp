#include "binder/query/updating_clause/bound_set_clause.h"

namespace kuzu {
namespace binder {

BoundSetClause::BoundSetClause(const BoundSetClause& other)
    : BoundUpdatingClause{common::ClauseType::SET} {
    for (auto& setPropertyInfo : other.setPropertyInfos) {
        setPropertyInfos.push_back(setPropertyInfo->copy());
    }
}

} // namespace binder
} // namespace kuzu
