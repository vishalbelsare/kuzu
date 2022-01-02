#include "src/processor/include/physical_plan/operator/scan_attribute/adj_column_extend.h"

namespace graphflow {
namespace processor {

shared_ptr<ResultSet> AdjColumnExtend::initResultSet() {
    ScanAttribute::initResultSet();
    outValueVector = make_shared<ValueVector>(context.memoryManager, NODE);
    inDataChunk->insert(outDataPos.valueVectorPos, outValueVector);
    return resultSet;
}

void AdjColumnExtend::reInitToRerunSubPlan() {
    ScanAttribute::reInitToRerunSubPlan();
    FilteringOperator::reInitToRerunSubPlan();
}

bool AdjColumnExtend::getNextTuples() {
    metrics->executionTime.start();
    bool hasAtLeastOneNonNullValue;
    do {
        restoreDataChunkSelectorState(inDataChunk);
        if (!children[0]->getNextTuples()) {
            metrics->executionTime.stop();
            return false;
        }
        saveDataChunkSelectorState(inDataChunk);
        outValueVector->setAllNull();
        column->readValues(inValueVector, outValueVector, *metrics->bufferManagerMetrics);
        hasAtLeastOneNonNullValue = outValueVector->discardNullNodes();
    } while (!hasAtLeastOneNonNullValue);
    metrics->executionTime.stop();
    metrics->numOutputTuple.increase(inDataChunk->state->selectedSize);
    return true;
}

} // namespace processor
} // namespace graphflow
