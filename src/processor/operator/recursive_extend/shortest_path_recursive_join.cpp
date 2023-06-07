#include "processor/operator/recursive_extend/shortest_path_recursive_join.h"

namespace kuzu {
namespace processor {

void ShortestPathRecursiveJoin::initLocalStateInternal(
    ResultSet* resultSet_, ExecutionContext* context) {
    BaseRecursiveJoin::initLocalStateInternal(resultSet_, context);
    distanceVector = resultSet->getValueVector(distanceVectorPos);
    maxOffset = nodeTable->getMaxNodeOffset(transaction);
    bfsMorsel = nullptr;
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
        auto duration = std::chrono::system_clock::now().time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        printf("SSSP with src: %lu completed in: %lu ms\n", bfsMorsel->srcOffset,
            millis - bfsMorsel->startTimeInMillis);
        isSSSPComplete = false;
        return false;
    }
}

} // namespace processor
} // namespace kuzu
