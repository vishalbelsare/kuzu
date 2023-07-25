#pragma once

#include "bound_set_info.h"
#include "bound_updating_clause.h"

namespace kuzu {
namespace binder {

class BoundSetClause : public BoundUpdatingClause {
public:
    BoundSetClause() : BoundUpdatingClause{common::ClauseType::SET} {}
    BoundSetClause(const BoundSetClause& other);

    inline void addSetPropertyInfo(std::unique_ptr<BoundSetPropertyInfo> setPropertyInfo) {
        setPropertyInfos.push_back(std::move(setPropertyInfo));
    }

    inline std::unique_ptr<BoundUpdatingClause> copy() final {
        return std::make_unique<BoundSetClause>(*this);
    }

private:
    std::vector<std::unique_ptr<BoundSetPropertyInfo>> setPropertyInfos;
};

} // namespace binder
} // namespace kuzu
