#pragma once

#include "common/ser_deser.h"
#include "common/types/value.h"
#include "parsed_expression.h"

namespace kuzu {
namespace parser {

class ParsedLiteralExpression : public ParsedExpression {
public:
    ParsedLiteralExpression(std::unique_ptr<common::Value> value, std::string raw)
        : ParsedExpression{common::LITERAL, std::move(raw)}, value{std::move(value)} {}

    ParsedLiteralExpression(std::string alias, std::string rawName,
        parsed_expression_vector children, std::unique_ptr<common::Value> value)
        : ParsedExpression{common::ExpressionType::LITERAL, std::move(alias), std::move(rawName),
              std::move(children)},
          value{std::move(value)} {}

    explicit ParsedLiteralExpression(std::unique_ptr<common::Value> value)
        : ParsedExpression{common::ExpressionType::LITERAL}, value{std::move(value)} {}

    inline common::Value* getValue() const { return value.get(); }

    static inline std::unique_ptr<ParsedLiteralExpression> deserialize(
        common::FileInfo* fileInfo, uint64_t& offset) {
        return std::make_unique<ParsedLiteralExpression>(
            common::Value::deserialize(fileInfo, offset));
    }

    inline std::unique_ptr<ParsedExpression> copy() const override {
        return std::make_unique<ParsedLiteralExpression>(
            alias, rawName, copyChildren(), value->copy());
    }

private:
    void serializeInternal(common::FileInfo* fileInfo, uint64_t& offset) const override {
        value->serialize(fileInfo, offset);
    }

private:
    std::unique_ptr<common::Value> value;
};

} // namespace parser
} // namespace kuzu
