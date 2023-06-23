#include "processor/operator/call.h"

namespace kuzu {
namespace processor {

bool Call::getNextTuplesInternal(kuzu::processor::ExecutionContext* context) {
    if (localState->hasExecuted) {
        return false;
    }
    localState->hasExecuted = true;
    auto optionValueExpression =
        reinterpret_cast<binder::LiteralExpression*>(localState->optionValue.get());
    localState->option.setContext(context->clientContext, *optionValueExpression->getValue());
    metrics->numOutputTuple.increase(1);
    return true;
}

} // namespace processor
} // namespace kuzu
