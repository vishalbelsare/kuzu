#pragma once

#include "bound_delete_info.h"
#include "bound_updating_clause.h"

namespace kuzu {
namespace binder {

class BoundDeleteClause : public BoundUpdatingClause {
public:
    BoundDeleteClause() : BoundUpdatingClause{common::ClauseType::DELETE_} {};

    inline void addDeleteNode(std::unique_ptr<BoundDeleteNode> deleteNode) {
        deleteNodes.push_back(std::move(deleteNode));
    }
    inline bool hasDeleteNode() const { return !deleteNodes.empty(); }
    inline uint32_t getNumDeleteNodes() const { return deleteNodes.size(); }
    inline BoundDeleteNode* getDeleteNode(uint32_t idx) const { return deleteNodes[idx].get(); }
    inline const std::vector<std::unique_ptr<BoundDeleteNode>>& getDeleteNodes() const {
        return deleteNodes;
    }

    inline void addDeleteRel(std::shared_ptr<RelExpression> deleteRel) {
        deleteRels.push_back(std::move(deleteRel));
    }
    inline bool hasDeleteRel() const { return !deleteRels.empty(); }
    inline uint32_t getNumDeleteRels() const { return deleteRels.size(); }
    inline std::shared_ptr<RelExpression> getDeleteRel(uint32_t idx) const {
        return deleteRels[idx];
    }
    inline std::vector<std::shared_ptr<RelExpression>> getDeleteRels() const { return deleteRels; }

    std::unique_ptr<BoundUpdatingClause> copy() override;

private:
    std::vector<std::unique_ptr<BoundDeleteNode>> deleteNodes;
    std::vector<std::shared_ptr<RelExpression>> deleteRels;
};

} // namespace binder
} // namespace kuzu
