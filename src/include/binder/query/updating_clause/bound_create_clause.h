#pragma once

#include "bound_create_info.h"
#include "bound_updating_clause.h"

namespace kuzu {
namespace binder {

class BoundCreateClause : public BoundUpdatingClause {
public:
    BoundCreateClause(std::vector<std::unique_ptr<BoundCreateNodeInfo>> createNodeInfos, std::vector<std::unique_ptr<BoundCreateRelInfo>> createRelInfos)
        : BoundUpdatingClause{common::ClauseType::CREATE}, createNodeInfos{std::move(createNodeInfos)}, createRelInfos{std::move(createRelInfos)} {}
    BoundCreateClause(const BoundCreateClause& other);


//    inline bool hasCreateNode() const { return !createNodes.empty(); }
//    inline const std::vector<std::unique_ptr<BoundCreateNode>>& getCreateNodes() const {
//        return createNodes;
//    }
//
//    inline bool hasCreateRel() const { return !createRels.empty(); }
//    inline const std::vector<std::unique_ptr<BoundCreateRel>>& getCreateRels() const {
//        return createRels;
//    }

    std::vector<expression_pair> getAllSetItems() const;

    std::unique_ptr<BoundUpdatingClause> copy() override;

private:
    std::vector<std::unique_ptr<BoundCreateNodeInfo>> createNodeInfos;
    std::vector<std::unique_ptr<BoundCreateRelInfo>> createRelInfos;
};

} // namespace binder
} // namespace kuzu
