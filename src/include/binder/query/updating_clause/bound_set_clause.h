#pragma once

#include "bound_set_info.h"
#include "bound_updating_clause.h"

namespace kuzu {
namespace binder {

class BoundSetClause : public BoundUpdatingClause {
public:
    BoundSetClause() : BoundUpdatingClause{common::ClauseType::SET} {}
    BoundSetClause(const BoundSetClause& other);

    inline void addSetNodeProperty(std::unique_ptr<BoundSetNodePropertyInfo> setNodeProperty) {
        setNodeProperties.push_back(std::move(setNodeProperty));
    }
    //    inline bool hasSetNodeProperty() const { return !setNodeProperties.empty(); }
    //    inline const std::vector<std::unique_ptr<BoundSetNodeProperty>>& getSetNodeProperties()
    //    const {
    //        return setNodeProperties;
    //    }

    inline void addSetRelProperty(std::unique_ptr<BoundSetRelProperty> setRelProperty) {
        setRelProperties.push_back(std::move(setRelProperty));
    }
    //    inline bool hasSetRelProperty() const { return !setRelProperties.empty(); }
    //    inline const std::vector<std::unique_ptr<BoundSetRelProperty>>& getSetRelProperties()
    //    const {
    //        return setRelProperties;
    //    }

    inline std::unique_ptr<BoundUpdatingClause> copy() final {
        return std::make_unique<BoundSetClause>(*this);
    }

private:
    std::vector<std::unique_ptr<BoundSetNodePropertyInfo>> setNodePropertyInfos;
    std::vector<std::unique_ptr<BoundSetRelPropertyInfo>> setRelPropertyInfos;
};

} // namespace binder
} // namespace kuzu
