#include "binder/binder.h"
#include "binder/call/bound_call.h"
#include "parser/call/call.h"

using namespace kuzu::common;
using namespace kuzu::parser;

namespace kuzu {
namespace binder {

std::unique_ptr<BoundStatement> Binder::bindCallClause(const parser::Statement& statement) {
    auto callStatement = reinterpret_cast<const Call&>(statement);
    auto option = main::DBConfig::getOptionByName(callStatement.getOptionName());
    if (option == nullptr) {
        throw BinderException{"Invalid option name: " + callStatement.getOptionName() + "."};
    }
    auto optionValue = expressionBinder.bindLiteralExpression(*callStatement.getOptionValue());
    // TODO(Ziyi): add casting rule for option value.
    if (optionValue->getDataType().getLogicalTypeID() != option->parameterType) {
        throw BinderException{
            StringUtils::string_format("Invalid option value type: {}. Expected: {}.",
                LogicalTypeUtils::dataTypeToString(optionValue->getDataType()),
                LogicalTypeUtils::dataTypeToString(option->parameterType))};
    }
    auto boundCall = std::make_unique<BoundCall>(*option, std::move(optionValue));
    return boundCall;
}

} // namespace binder
} // namespace kuzu
