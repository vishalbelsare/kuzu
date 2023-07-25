#pragma once

#include "binder/query/reading_clause/query_graph.h"
#include "bound_updating_clause.h"

namespace kuzu {
namespace binder {

class BoundMergeClause : public BoundUpdatingClause {
public:
    BoundMergeClause(std::unique_ptr<QueryGraphCollection> queryGraphCollection,
        std::vector<std::unique_ptr<BoundCreateNodeInfo>> createNodeInfos,
        std::vector<std::unique_ptr<BoundCreateRelInfo>> createRelInfos)
        : BoundUpdatingClause{common::ClauseType::MERGE}, queryGraphCollection{std::move(
                                                              queryGraphCollection)},
          createNodeInfos{std::move(createNodeInfos)}, createRelInfos{std::move(createRelInfos)} {}

private:
    // Pattern to match.
    std::unique_ptr<QueryGraphCollection> queryGraphCollection;
    // Pattern to create on match failure.
    std::vector<std::unique_ptr<BoundCreateNodeInfo>> createNodeInfos;
    std::vector<std::unique_ptr<BoundCreateRelInfo>> createRelInfos;
    // Update on match
    std::vector<std::unique_ptr<BoundSetNodePropertyInfo>> onMatchSetNodePropertyInfos;
    std::vector<std::unique_ptr<BoundSetRelPropertyInfo>> onMatchSetRelPropertyInfos;
    // Update on create

};

} // namespace binder
} // namespace kuzu