#pragma once

#include "bound_set_info.h"
#include "bound_updating_clause.h"

namespace kuzu {
namespace binder {

class BoundSetClause : public BoundUpdatingClause {
public:
    BoundSetClause() : BoundUpdatingClause{common::ClauseType::SET} {}

    inline void addSetNodeProperty(std::unique_ptr<BoundSetNodePropertyInfo> setNodeProperty) {
        setNodeProperties.push_back(std::move(setNodeProperty));
    }
    inline bool hasSetNodeProperty() const { return !setNodeProperties.empty(); }
    inline const std::vector<std::unique_ptr<BoundSetNodeProperty>>& getSetNodeProperties() const {
        return setNodeProperties;
    }

    inline void addSetRelProperty(std::unique_ptr<BoundSetRelProperty> setRelProperty) {
        setRelProperties.push_back(std::move(setRelProperty));
    }
    inline bool hasSetRelProperty() const { return !setRelProperties.empty(); }
    inline const std::vector<std::unique_ptr<BoundSetRelProperty>>& getSetRelProperties() const {
        return setRelProperties;
    }

    std::unique_ptr<BoundUpdatingClause> copy() override;

private:
    std::vector<std::unique_ptr<BoundSetNodeProperty>> setNodeProperties;
    std::vector<std::unique_ptr<BoundSetRelProperty>> setRelProperties;
};

} // namespace binder
} // namespace kuzu
