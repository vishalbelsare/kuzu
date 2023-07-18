#pragma once

#include "parser/query/graph_pattern/pattern_element.h"
#include "updating_clause.h"

namespace kuzu {
namespace parser {

class MergeClause : public UpdatingClause {
public:
    explicit MergeClause(std::vector<std::unique_ptr<PatternElement>> patternElements)
        : UpdatingClause{common::ClauseType::MERGE}, patternElements{std::move(patternElements)} {}

    inline const std::vector<std::unique_ptr<PatternElement>>& getPatternElementsRef() const {
        return patternElements;
    }
    inline void addMergeAction(std::vector<parsed_expression_pair> mergeAction) {
        mergeActions.push_back(std::move(mergeAction));
    }

private:
    std::vector<std::unique_ptr<PatternElement>> patternElements;
    std::vector<std::vector<parsed_expression_pair>> mergeActions;
};

} // namespace parser
} // namespace kuzu
