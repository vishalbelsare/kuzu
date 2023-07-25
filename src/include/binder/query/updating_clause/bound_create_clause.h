#pragma once

#include "bound_create_info.h"
#include "bound_updating_clause.h"

namespace kuzu {
namespace binder {

class BoundCreateClause : public BoundUpdatingClause {
public:
    BoundCreateClause(std::vector<std::unique_ptr<BoundCreateNodeInfo>> createNodeInfos,
        std::vector<std::unique_ptr<BoundCreateRelInfo>> createRelInfos)
        : BoundUpdatingClause{common::ClauseType::CREATE},
          createNodeInfos{std::move(createNodeInfos)}, createRelInfos{std::move(createRelInfos)} {}
    BoundCreateClause(const BoundCreateClause& other);

    inline bool hasNodeInfo() const { return !createNodeInfos.empty(); }
    std::vector<BoundCreateNodeInfo*> getNodeInfos() const;

    inline bool hasRelInfo() const { return !createRelInfos.empty(); }
    std::vector<BoundCreateRelInfo*> getRelInfos() const;

    std::vector<expression_pair> getAllSetItems() const;

    inline std::unique_ptr<BoundUpdatingClause> copy() final {
        return std::make_unique<BoundCreateClause>(*this);
    }

private:
    std::vector<std::unique_ptr<BoundCreateNodeInfo>> createNodeInfos;
    std::vector<std::unique_ptr<BoundCreateRelInfo>> createRelInfos;
};

} // namespace binder
} // namespace kuzu
