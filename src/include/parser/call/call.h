#pragma once

#include "parser/expression/parsed_expression.h"
#include "parser/statement.h"

namespace kuzu {
namespace parser {

class Call : public Statement {
public:
    explicit Call(std::string optionName, std::shared_ptr<ParsedExpression> optionValue)
        : Statement{common::StatementType::CALL}, optionName{std::move(optionName)},
          optionValue{optionValue} {}

    inline std::string getOptionName() const { return optionName; }

    inline std::shared_ptr<ParsedExpression> getOptionValue() const { return optionValue; }

private:
    std::string optionName;
    std::shared_ptr<ParsedExpression> optionValue;
};

} // namespace parser
} // namespace kuzu
