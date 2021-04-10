#pragma once

#include "src/processor/include/physical_plan/operator/physical_operator.h"

namespace graphflow {
namespace processor {

class Flatten : public PhysicalOperator {

public:
    Flatten(uint64_t dataChunkToFlattenPos, unique_ptr<PhysicalOperator> prevOperator);

    void getNextTuples() override;

    unique_ptr<PhysicalOperator> clone() override {
        return make_unique<Flatten>(dataChunkToFlattenPos, prevOperator->clone());
    }

private:
    uint64_t dataChunkToFlattenPos;
    shared_ptr<DataChunk> dataChunkToFlatten;
};

} // namespace processor
} // namespace graphflow
