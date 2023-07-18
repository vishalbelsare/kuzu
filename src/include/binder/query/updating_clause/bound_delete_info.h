#pragma once

#include "binder/expression/rel_expression.h"

namespace kuzu {
namespace binder {

struct BoundDeleteNodeInfo {
    std::shared_ptr<NodeExpression> node;
    std::shared_ptr<Expression> primaryKey;

    BoundDeleteNodeInfo(
        std::shared_ptr<NodeExpression> node, std::shared_ptr<Expression> primaryKey)
        : node{std::move(node)}, primaryKey{std::move(primaryKey)} {}
    BoundDeleteNodeInfo(const BoundDeleteNodeInfo& other)
        : node{other.node}, primaryKey{other.primaryKey} {}

    inline std::unique_ptr<BoundDeleteNodeInfo> copy() {
        return std::make_unique<BoundDeleteNodeInfo>(*this);
    }
};

} // namespace binder
} // namespace kuzu
