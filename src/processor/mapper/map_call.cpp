#include "planner/logical_plan/logical_operator/logical_call.h"
#include "processor/mapper/plan_mapper.h"
#include "processor/operator/call.h"

using namespace kuzu::planner;

namespace kuzu {
namespace processor {

std::unique_ptr<PhysicalOperator> PlanMapper::mapLogicalCallToPhysical(
    planner::LogicalOperator* logicalOperator) {
    auto logicalCall = reinterpret_cast<LogicalCall*>(logicalOperator);
    auto callLocalState =
        std::make_unique<CallLocalState>(logicalCall->getOption(), logicalCall->getOptionValue());
    return std::make_unique<Call>(std::move(callLocalState), PhysicalOperatorType::CALL,
        getOperatorID(), logicalCall->getExpressionsForPrinting());
}

} // namespace processor
} // namespace kuzu
