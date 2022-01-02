#pragma once

#include "src/processor/include/physical_plan/operator/read_list/frontier/frontier_set.h"
#include "src/processor/include/physical_plan/operator/read_list/read_list.h"

namespace graphflow {
namespace processor {

class FrontierExtend : public ReadList {

public:
    FrontierExtend(const DataPos& inDataPos, const DataPos& outDataPos, AdjLists* lists,
        label_t outNodeIDVectorLabel, uint64_t lowerBound, uint64_t upperBound,
        unique_ptr<PhysicalOperator> child, ExecutionContext& context, uint32_t id);

    PhysicalOperatorType getOperatorType() override { return FRONTIER_EXTEND; }

    shared_ptr<ResultSet> initResultSet() override;

    bool getNextTuples() override;

    unique_ptr<PhysicalOperator> clone() override {
        return make_unique<FrontierExtend>(inDataPos, outDataPos, (AdjLists*)lists,
            outNodeIDVectorLabel, startLayer, endLayer, children[0]->clone(), context, id);
    }

private:
    bool computeFrontiers();
    void produceOutputTuples();
    FrontierBag* createFrontierBag();
    FrontierSet* createInitialFrontierSet();
    FrontierSet* makeFrontierSet(uint64_t layer);
    void createGlobalFrontierFromThreadLocalFrontiers(uint64_t layer);
    void extendToThreadLocalFrontiers(uint64_t layer);

private:
    uint64_t startLayer;
    uint64_t endLayer;
    vector<FrontierSet*> frontierPerLayer;
    vector<vector<FrontierBag*>> threadLocalFrontierPerLayer;
    // We create a vector and largeListHandle per thread assuming omp_get_max_threads() threads.
    // Each thread uses their vector and LargeListHandle for uncoordinated extensions.
    vector<shared_ptr<ValueVector>> vectors;
    vector<unique_ptr<LargeListHandle>> largeListHandles;

    struct CurrentOutputPosition {
        bool hasMoreTuplesToProduce;
        uint64_t layer;
        uint64_t blockIdx;
        uint64_t slot;

        void reset(uint64_t startLayer) {
            hasMoreTuplesToProduce = false;
            layer = startLayer;
            blockIdx = slot = 0u;
        }
    } currOutputPos;

    label_t outNodeIDVectorLabel;
};

} // namespace processor
} // namespace graphflow
