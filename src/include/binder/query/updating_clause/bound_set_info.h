#pragma once

#include "binder/expression/rel_expression.h"

namespace kuzu {
namespace binder {

enum class SetPropertyType : uint8_t {
    NODE = 0,
    REL = 1,
};

struct BoundSetPropertyInfo {
    SetPropertyType setPropertyType;
    std::shared_ptr<Expression> nodeOrRel;
    expression_pair setItem;

    BoundSetPropertyInfo(SetPropertyType setPropertyType, std::shared_ptr<Expression> nodeOrRel,
        expression_pair setItem)
        : setPropertyType{setPropertyType}, nodeOrRel{std::move(nodeOrRel)}, setItem{std::move(
                                                                                 setItem)} {}
    BoundSetPropertyInfo(const BoundSetPropertyInfo& other)
        : setPropertyType{other.setPropertyType}, nodeOrRel{other.nodeOrRel}, setItem{
                                                                                  other.setItem} {}

    inline std::unique_ptr<BoundSetPropertyInfo> copy() const {
        return std::make_unique<BoundSetPropertyInfo>(*this);
    }
};

} // namespace binder
} // namespace kuzu
