#pragma once

#include "binder/query/reading_clause/query_graph.h"
#include "bound_updating_clause.h"

namespace kuzu {
namespace binder {

class BoundMergeClause : public BoundUpdatingClause {
public:
    explicit BoundMergeClause(std::unique_ptr<QueryGraphCollection> queryGraphCollection)
        : BoundUpdatingClause{common::ClauseType::MERGE}, queryGraphCollection{
                                                              std::move(queryGraphCollection)} {}

private:
    std::unique_ptr<QueryGraphCollection> queryGraphCollection;
    std::vector<std::unique_ptr<BoundCreateNodeInfo>> createNodeInfos;
    std::vector<std::unique_ptr<BoundCreateRelInfo>> createRelInfos;
};

} // namespace binder
} // namespace kuzu