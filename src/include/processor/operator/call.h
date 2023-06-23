#pragma once

#include "binder/expression/literal_expression.h"
#include "main/db_config.h"
#include "processor/operator/physical_operator.h"

namespace kuzu {
namespace processor {

struct CallLocalState {
    main::ConfigurationOption option;
    std::shared_ptr<binder::Expression> optionValue;
    bool hasExecuted = false;

    CallLocalState(
        main::ConfigurationOption option, std::shared_ptr<binder::Expression> optionValue)
        : option{std::move(option)}, optionValue{std::move(optionValue)} {}

    std::unique_ptr<CallLocalState> copy() {
        return std::make_unique<CallLocalState>(option, optionValue);
    }
};

class Call : public PhysicalOperator {
public:
    Call(std::unique_ptr<CallLocalState> localState, PhysicalOperatorType operatorType, uint32_t id,
        const std::string& paramsString)
        : localState{std::move(localState)}, PhysicalOperator{operatorType, id, paramsString} {}

    inline bool isSource() const override { return true; }

    bool getNextTuplesInternal(ExecutionContext* context) override;

    inline std::unique_ptr<PhysicalOperator> clone() override {
        return std::make_unique<Call>(localState->copy(), operatorType, id, paramsString);
    }

protected:
    std::unique_ptr<CallLocalState> localState;
};

} // namespace processor
} // namespace kuzu
