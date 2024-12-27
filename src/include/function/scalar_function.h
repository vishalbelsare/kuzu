#pragma once

#include "binary_function_executor.h"
#include "const_function_executor.h"
#include "function.h"
#include "pointer_function_executor.h"
#include "ternary_function_executor.h"
#include "unary_function_executor.h"

namespace kuzu {
namespace function {

// Evaluate function at compile time, e.g. struct_extraction.
using scalar_func_compile_exec_t =
    std::function<void(FunctionBindData*, const std::vector<std::shared_ptr<common::ValueVector>>&,
        std::shared_ptr<common::ValueVector>&)>;
// Execute function.
using scalar_func_exec_t = std::function<void(
    const std::vector<std::shared_ptr<common::ValueVector>>&, common::ValueVector&, void*)>;
// Execute boolean function and write result to selection vector. Fast path for filter.
using scalar_func_select_t = std::function<bool(
    const std::vector<std::shared_ptr<common::ValueVector>>&, common::SelectionVector&)>;

struct ScalarFunction : public ScalarOrAggregateFunction {
    scalar_func_exec_t execFunc = nullptr;
    scalar_func_select_t selectFunc = nullptr;
    scalar_func_compile_exec_t compileFunc = nullptr;

    ScalarFunction() = default;
    ScalarFunction(std::string name, std::vector<common::LogicalTypeID> parameterTypeIDs,
        common::LogicalTypeID returnTypeID)
        : ScalarOrAggregateFunction{std::move(name), std::move(parameterTypeIDs), returnTypeID} {}
    ScalarFunction(std::string name, std::vector<common::LogicalTypeID> parameterTypeIDs,
        common::LogicalTypeID returnTypeID, scalar_func_exec_t execFunc)
        : ScalarOrAggregateFunction{std::move(name), std::move(parameterTypeIDs), returnTypeID},
          execFunc{execFunc} {}
    ScalarFunction(std::string name, std::vector<common::LogicalTypeID> parameterTypeIDs,
        common::LogicalTypeID returnTypeID, scalar_func_exec_t execFunc,
        scalar_func_select_t selectFunc)
        : ScalarOrAggregateFunction{std::move(name), std::move(parameterTypeIDs), returnTypeID},
          execFunc{execFunc}, selectFunc{selectFunc} {}

    template<typename A_TYPE, typename B_TYPE, typename C_TYPE, typename RESULT_TYPE, typename FUNC>
    static void TernaryExecFunction(const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* /*dataPtr*/ = nullptr) {
        KU_ASSERT(params.size() == 3);
        TernaryFunctionExecutor::execute<A_TYPE, B_TYPE, C_TYPE, RESULT_TYPE, FUNC>(*params[0],
            *params[1], *params[2], result);
    }

    template<typename A_TYPE, typename B_TYPE, typename C_TYPE, typename RESULT_TYPE, typename FUNC>
    static void TernaryStringExecFunction(
        const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* /*dataPtr*/ = nullptr) {
        KU_ASSERT(params.size() == 3);
        TernaryFunctionExecutor::executeString<A_TYPE, B_TYPE, C_TYPE, RESULT_TYPE, FUNC>(
            *params[0], *params[1], *params[2], result);
    }

    template<typename A_TYPE, typename B_TYPE, typename C_TYPE, typename RESULT_TYPE, typename FUNC>
    static void TernaryRegexExecFunction(
        const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* dataPtr) {
        TernaryFunctionExecutor::executeRegex<A_TYPE, B_TYPE, C_TYPE, RESULT_TYPE, FUNC>(*params[0],
            *params[1], *params[2], result, dataPtr);
    }

    template<typename LEFT_TYPE, typename RIGHT_TYPE, typename RESULT_TYPE, typename FUNC>
    static void BinaryExecFunction(const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* /*dataPtr*/ = nullptr) {
        KU_ASSERT(params.size() == 2);
        BinaryFunctionExecutor::execute<LEFT_TYPE, RIGHT_TYPE, RESULT_TYPE, FUNC>(*params[0],
            *params[1], result);
    }

    template<typename LEFT_TYPE, typename RIGHT_TYPE, typename RESULT_TYPE, typename FUNC>
    static void BinaryStringExecFunction(
        const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* /*dataPtr*/ = nullptr) {
        KU_ASSERT(params.size() == 2);
        BinaryFunctionExecutor::executeString<LEFT_TYPE, RIGHT_TYPE, RESULT_TYPE, FUNC>(*params[0],
            *params[1], result);
    }

    template<typename LEFT_TYPE, typename RIGHT_TYPE, typename FUNC>
    static bool BinarySelectFunction(
        const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::SelectionVector& selVector) {
        KU_ASSERT(params.size() == 2);
        return BinaryFunctionExecutor::select<LEFT_TYPE, RIGHT_TYPE, FUNC>(*params[0], *params[1],
            selVector);
    }

    template<typename OPERAND_TYPE, typename RESULT_TYPE, typename FUNC,
        typename EXECUTOR = UnaryFunctionExecutor>
    static void UnaryExecFunction(const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* dataPtr) {
        KU_ASSERT(params.size() == 1);
        EXECUTOR::template executeSwitch<OPERAND_TYPE, RESULT_TYPE, FUNC, UnaryFunctionWrapper>(
            *params[0], result, dataPtr);
    }

    template<typename OPERAND_TYPE, typename RESULT_TYPE, typename FUNC>
    static void UnarySequenceExecFunction(
        const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* dataPtr) {
        KU_ASSERT(params.size() == 1);
        UnaryFunctionExecutor::executeSequence<OPERAND_TYPE, RESULT_TYPE, FUNC>(*params[0], result,
            dataPtr);
    }

    template<typename OPERAND_TYPE, typename RESULT_TYPE, typename FUNC>
    static void UnaryStringExecFunction(
        const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* /*dataPtr*/ = nullptr) {
        KU_ASSERT(params.size() == 1);
        UnaryFunctionExecutor::executeSwitch<OPERAND_TYPE, RESULT_TYPE, FUNC,
            UnaryStringFunctionWrapper>(*params[0], result, nullptr /* dataPtr */);
    }

    template<typename OPERAND_TYPE, typename RESULT_TYPE, typename FUNC,
        typename EXECUTOR = UnaryFunctionExecutor>
    static void UnaryCastStringExecFunction(
        const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* dataPtr) {
        KU_ASSERT(params.size() == 1);
        EXECUTOR::template executeSwitch<OPERAND_TYPE, RESULT_TYPE, FUNC,
            UnaryCastStringFunctionWrapper>(*params[0], result, dataPtr);
    }

    template<typename OPERAND_TYPE, typename RESULT_TYPE, typename FUNC,
        typename EXECUTOR = UnaryFunctionExecutor>
    static void UnaryCastExecFunction(
        const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* dataPtr) {
        KU_ASSERT(params.size() == 1);
        EXECUTOR::template executeSwitch<OPERAND_TYPE, RESULT_TYPE, FUNC, UnaryCastFunctionWrapper>(
            *params[0], result, dataPtr);
    }

    template<typename OPERAND_TYPE, typename RESULT_TYPE, typename FUNC>
    static void UnaryExecNestedTypeFunction(
        const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* /*dataPtr*/ = nullptr) {
        KU_ASSERT(params.size() == 1);
        UnaryFunctionExecutor::executeSwitch<OPERAND_TYPE, RESULT_TYPE, FUNC,
            UnaryNestedTypeFunctionWrapper>(*params[0], result, nullptr /* dataPtr */);
    }

    template<typename OPERAND_TYPE, typename RESULT_TYPE, typename FUNC>
    static void UnaryExecStructFunction(
        const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* dataPtr) {
        KU_ASSERT(params.size() == 1);
        UnaryFunctionExecutor::executeSwitch<OPERAND_TYPE, RESULT_TYPE, FUNC,
            UnaryStructFunctionWrapper>(*params[0], result, dataPtr);
    }

    template<typename RESULT_TYPE, typename FUNC>
    static void NullaryExecFunction(const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* /*dataPtr*/ = nullptr) {
        KU_ASSERT(params.empty());
        (void)params;
        ConstFunctionExecutor::execute<RESULT_TYPE, FUNC>(result);
    }

    template<typename RESULT_TYPE, typename FUNC>
    static void NullaryAuxilaryExecFunction(
        const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* dataPtr) {
        KU_ASSERT(params.empty());
        (void)params;
        PointerFunctionExecutor::execute<RESULT_TYPE, FUNC>(result, dataPtr);
    }

    template<typename A_TYPE, typename B_TYPE, typename C_TYPE, typename RESULT_TYPE, typename FUNC>
    static void TernaryExecListStructFunction(
        const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* /*dataPtr*/ = nullptr) {
        KU_ASSERT(params.size() == 3);
        TernaryFunctionExecutor::executeListStruct<A_TYPE, B_TYPE, C_TYPE, RESULT_TYPE, FUNC>(
            *params[0], *params[1], *params[2], result);
    }

    template<typename LEFT_TYPE, typename RIGHT_TYPE, typename RESULT_TYPE, typename FUNC>
    static void BinaryExecListStructFunction(
        const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* /*dataPtr*/ = nullptr) {
        KU_ASSERT(params.size() == 2);
        BinaryFunctionExecutor::executeListStruct<LEFT_TYPE, RIGHT_TYPE, RESULT_TYPE, FUNC>(
            *params[0], *params[1], result);
    }

    template<typename LEFT_TYPE, typename RIGHT_TYPE, typename RESULT_TYPE, typename FUNC>
    static void BinaryExecMapCreationFunction(
        const std::vector<std::shared_ptr<common::ValueVector>>& params,
        common::ValueVector& result, void* dataPtr) {
        KU_ASSERT(params.size() == 2);
        BinaryFunctionExecutor::executeMapCreation<LEFT_TYPE, RIGHT_TYPE, RESULT_TYPE, FUNC>(
            *params[0], *params[1], result, dataPtr);
    }

    virtual std::unique_ptr<ScalarFunction> copy() const {
        return std::make_unique<ScalarFunction>(*this);
    }
};

} // namespace function
} // namespace kuzu
