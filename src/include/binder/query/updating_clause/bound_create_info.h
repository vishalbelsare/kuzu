#pragma once

#include "binder/expression/rel_expression.h"

namespace kuzu {
namespace binder {

struct BoundCreateNodeInfo {
    std::shared_ptr<NodeExpression> node;
    std::shared_ptr<Expression> primaryKey;
    std::vector<expression_pair> setItems;

    BoundCreateNodeInfo(std::shared_ptr<NodeExpression> node,
        std::shared_ptr<Expression> primaryKey, std::vector<expression_pair> setItems)
        : node{std::move(node)}, primaryKey{std::move(primaryKey)}, setItems{std::move(setItems)} {}
    BoundCreateNodeInfo(const BoundCreateNodeInfo& other)
        : node{other.node}, primaryKey{other.primaryKey}, setItems{other.setItems} {}

    inline std::unique_ptr<BoundCreateNodeInfo> copy() const {
        return std::make_unique<BoundCreateNodeInfo>(*this);
    }
};

struct BoundCreateRelInfo {
    std::shared_ptr<RelExpression> rel;
    std::vector<expression_pair> setItems;

    BoundCreateRelInfo(std::shared_ptr<RelExpression> rel, std::vector<expression_pair> setItems)
        : rel{std::move(rel)}, setItems{std::move(setItems)} {}
    BoundCreateRelInfo(const BoundCreateRelInfo& other)
        : rel{other.rel}, setItems{other.setItems} {}

    inline std::unique_ptr<BoundCreateRelInfo> copy() const {
        return std::make_unique<BoundCreateRelInfo>(*this);
    }
};

} // namespace binder
} // namespace kuzu
