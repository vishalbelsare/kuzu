#include "processor/operator/recursive_extend/shortest_path_recursive_join.h"

namespace kuzu {
namespace processor {

void ShortestPathRecursiveJoin::initLocalStateInternal(
    ResultSet* resultSet_, ExecutionContext* context) {
    BaseRecursiveJoin::initLocalStateInternal(resultSet_, context);
    distanceVector = resultSet->getValueVector(distanceVectorPos);
    auto maxNodeOffset = nodeTable->getMaxNodeOffset(transaction);
    bfsMorsel = std::make_unique<ShortestPathBFSMorsel>(
        maxNodeOffset, lowerBound, upperBound, sharedState->semiMask.get());
    bfsMorsel->resetState();
}

bool ShortestPathRecursiveJoin::scanOutput() {
    auto morsel = (ShortestPathBFSMorsel*)bfsMorsel.get();
    // if sssp is not complete, nothing to write so return false
    if (!isSSSPComplete) {
        return false;
    }
    auto vectorSize = 0u;
    while (vectorSize != common::DEFAULT_VECTOR_CAPACITY &&
           outputCursor < morsel->dstNodeOffset2PathLength.size()) {
        if (morsel->dstNodeOffset2PathLength[outputCursor] >= lowerBound) {
            dstNodeIDVector->setValue<common::nodeID_t>(
                vectorSize, common::nodeID_t{outputCursor, nodeTable->getTableID()});
            distanceVector->setValue<int64_t>(vectorSize,
                morsel->dstNodeOffset2PathLength[outputCursor]);
            vectorSize++;
        }
        outputCursor++;
    }
    if(vectorSize > 0) {
        dstNodeIDVector->state->initOriginalAndSelectedSize(vectorSize);
        return true;
    } else {
        isSSSPComplete = false;
        return false;
    }
}

} // namespace processor
} // namespace kuzu
