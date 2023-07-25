#include "binder/query/updating_clause/bound_set_clause.h"

namespace kuzu {
namespace binder {

BoundSetClause::BoundSetClause(const BoundSetClause& other)
    : BoundUpdatingClause{common::ClauseType::SET} {
    for (auto& setNodePropertyInfo : other.setNodePropertyInfos) {
        setNodePropertyInfos.push_back(setNodePropertyInfo->copy());
    }
    for (auto& setRelPropertyInfo : other.setRelPropertyInfos) {
        setRelPropertyInfos.push_back(setRelPropertyInfo->copy());
    }
}

} // namespace binder
} // namespace kuzu
