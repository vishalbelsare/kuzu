#include "processor/operator/recursive_extend/bfs_state.h"

namespace kuzu {
namespace processor {

common::offset_t BaseBFSMorsel::getNextNodeOffset() {
    if (nextNodeIdxToExtend == bfsLevelNodeOffsets.size()) {
        return common::INVALID_OFFSET;
    }
    return bfsLevelNodeOffsets[nextNodeIdxToExtend++];
}

void BaseBFSMorsel::moveNextLevelAsCurrentLevel() {
    currentLevel++;
    nextNodeIdxToExtend = 0;
    bfsLevelNodeOffsets.clear();
    auto ssspMorsel = (ShortestPathBFSMorsel*)this;
    if (currentLevel < upperBound) { // No need to prepare if we are not extending further.
        auto duration = std::chrono::system_clock::now().time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        for(common::offset_t i = 0u; i < (maxOffset+1); i++) {
            if(ssspMorsel->visitedNodes[i] == VISITED_DST_NEW) {
                ssspMorsel->visitedNodes[i] = VISITED_DST;
                bfsLevelNodeOffsets.push_back(i);
                printf("marking: %lu as VISITED_DST\n", i);
            } else if(ssspMorsel->visitedNodes[i] == VISITED_NEW) {
                ssspMorsel->visitedNodes[i] = VISITED;
                bfsLevelNodeOffsets.push_back(i);
            }
        }
        auto duration1 = std::chrono::system_clock::now().time_since_epoch();
        auto millis1 = std::chrono::duration_cast<std::chrono::milliseconds>(duration1).count();
        printf("Time taken: %lu to prepare for level: %u by scanning: %lu offsets\n",
            (millis1 - millis), currentLevel, maxOffset+1);
    }
    auto duration = std::chrono::system_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    printf("Moving to level: %u for source: %lu, at time: %lu\n", currentLevel, srcOffset, millis);
}

void BaseBFSMorsel::resetState() {
    currentLevel = 0;
    nextNodeIdxToExtend = 0;
    bfsLevelNodeOffsets.clear();
    numTargetDstNodes = 0;
    startTimeInMillis = 0u;
    srcOffset = 0u;
}

void ShortestPathBFSMorsel::markSrc(common::offset_t offset) {
    if (visitedNodes[offset] == NOT_VISITED_DST) {
        visitedNodes[offset] = VISITED_DST;
        numVisitedDstNodes++;
    } else {
        visitedNodes[offset] = VISITED;
    }
    bfsLevelNodeOffsets.push_back(offset);
}

void ShortestPathBFSMorsel::markVisited(common::offset_t offset, uint64_t multiplicity) {
    if (visitedNodes[offset] == NOT_VISITED_DST) {
        visitedNodes[offset] = VISITED_DST_NEW;
        dstNodeOffset2PathLength[offset] = currentLevel + 1;
        numVisitedDstNodes++;
    } else if (visitedNodes[offset] == NOT_VISITED) {
        visitedNodes[offset] = VISITED_NEW;
    }
}

void ShortestPathBFSMorsel::resetVisitedState() {
    numVisitedDstNodes = 0;
    std::fill(dstNodeOffset2PathLength.begin(), dstNodeOffset2PathLength.end(), 0u);
    if (!isAllDstTarget()) {
        std::fill(visitedNodes, visitedNodes + maxOffset + 1, (uint8_t)VisitedState::NOT_VISITED);
        for (auto offset : targetDstNodeOffsets) {
            visitedNodes[offset] = VisitedState::NOT_VISITED_DST;
        }
        numTargetDstNodes = targetDstNodeOffsets.size();
    } else {
        std::fill(
            visitedNodes, visitedNodes + maxOffset + 1, (uint8_t)VisitedState::NOT_VISITED_DST);
        numTargetDstNodes = maxOffset + 1;
    }
}

/*void VariableLengthBFSMorsel::updateNumPathFromCurrentFrontier() {
    if (currentLevel < lowerBound) {
        return;
    }
    if (!isAllDstTarget() && numTargetDstNodes < currentFrontier->nodeOffsets.size()) {
        // Target is smaller than current frontier size. Loop through target instead of current
        // frontier.
        for (auto offset : targetDstNodeOffsets) {
            if (((FrontierWithMultiplicity&)*currentFrontier).contains(offset)) {
                updateNumPath(offset, currentFrontier->getMultiplicity(offset));
            }
        }
    } else {
        for (auto offset : currentFrontier->nodeOffsets) {
            updateNumPath(offset, currentFrontier->getMultiplicity(offset));
        }
    }
}*/

} // namespace processor
} // namespace kuzu
