#pragma once

#include "binder/expression/rel_expression.h"

namespace kuzu {
namespace binder {

enum class SetPropertyType;

//struct BoundSetNodePropertyInfo {
//    std::shared_ptr<NodeExpression> node;
//    expression_pair setItem;
//
//    BoundSetNodePropertyInfo(std::shared_ptr<NodeExpression> node, expression_pair setItem)
//        : node{std::move(node)}, setItem{std::move(setItem)} {}
//    BoundSetNodePropertyInfo(const BoundSetNodePropertyInfo& other)
//        : node{other.node}, setItem{other.setItem} {}
//
//    inline std::unique_ptr<BoundSetNodePropertyInfo> copy() const {
//        return std::make_unique<BoundSetNodePropertyInfo>(*this);
//    }
//};
//
//struct BoundSetRelPropertyInfo {
//    std::shared_ptr<RelExpression> rel;
//    expression_pair setItem;
//
//    BoundSetRelPropertyInfo(std::shared_ptr<RelExpression> rel, expression_pair setItem)
//        : rel{std::move(rel)}, setItem{std::move(setItem)} {}
//    BoundSetRelPropertyInfo(const BoundSetRelPropertyInfo& other)
//        : rel{other.rel}, setItem{other.setItem} {}
//
//    inline std::unique_ptr<BoundSetRelPropertyInfo> copy() const {
//        return std::make_unique<BoundSetRelPropertyInfo>(*this);
//    }
//};

} // namespace binder
} // namespace kuzu
