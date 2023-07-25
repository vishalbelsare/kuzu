#pragma once

#include "bound_delete_info.h"
#include "bound_updating_clause.h"

namespace kuzu {
namespace binder {

class BoundDeleteClause : public BoundUpdatingClause {
public:
    BoundDeleteClause() : BoundUpdatingClause{common::ClauseType::DELETE_} {};
    BoundDeleteClause(const BoundDeleteClause& other);

    inline void addDeleteNode(std::unique_ptr<BoundDeleteNodeInfo> deleteNodeInfo) {
        deleteNodeInfos.push_back(std::move(deleteNodeInfo));
    }
    //    inline bool hasDeleteNode() const { return !deleteNodes.empty(); }
    //    inline uint32_t getNumDeleteNodes() const { return deleteNodes.size(); }
    //    inline BoundDeleteNode* getDeleteNode(uint32_t idx) const { return deleteNodes[idx].get();
    //    } inline const std::vector<std::unique_ptr<BoundDeleteNode>>& getDeleteNodes() const {
    //        return deleteNodes;
    //    }

    inline void addDeleteRel(std::shared_ptr<RelExpression> deleteRel) {
        deleteRels.push_back(std::move(deleteRel));
    }
    inline bool hasDeleteRel() const { return !deleteRels.empty(); }
    inline std::vector<std::shared_ptr<RelExpression>> getDeleteRels() const { return deleteRels; }

    inline std::unique_ptr<BoundUpdatingClause> copy() final {
        return std::make_unique<BoundDeleteClause>(*this);
    }

private:
    std::vector<std::unique_ptr<BoundDeleteNodeInfo>> deleteNodeInfos;
    std::vector<std::shared_ptr<RelExpression>> deleteRels;
};

} // namespace binder
} // namespace kuzu
