#include "src/processor/include/physical_plan/operator/multiplicity_reducer.h"

namespace graphflow {
namespace processor {

shared_ptr<ResultSet> MultiplicityReducer::initResultSet() {
    resultSet = children[0]->initResultSet();
    return resultSet;
}

void MultiplicityReducer::reInitToRerunSubPlan() {
    children[0]->reInitToRerunSubPlan();
    prevMultiplicity = 1;
    numRepeat = 0;
}

bool MultiplicityReducer::getNextTuples() {
    metrics->executionTime.start();
    if (numRepeat == 0) {
        restoreMultiplicity();
        if (!children[0]->getNextTuples()) {
            metrics->executionTime.stop();
            return false;
        }
        saveMultiplicity();
        resultSet->multiplicity = 1;
    }
    numRepeat++;
    if (numRepeat == prevMultiplicity) {
        numRepeat = 0;
    }
    metrics->executionTime.stop();
    return true;
}

} // namespace processor
} // namespace graphflow
