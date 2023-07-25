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
    inline void addOnMatchAction(parsed_expression_pair action) {
        onMatchActions.push_back(std::move(action));
    }
    inline bool hasOnMatchActions() const {
        return !onMatchActions.empty();
    }
    inline void addOnCreateAction(parsed_expression_pair action) {
        onCreateActions.push_back(std::move(action));
    }

private:
    std::vector<std::unique_ptr<PatternElement>> patternElements;
    std::vector<parsed_expression_pair> onMatchActions;
    std::vector<parsed_expression_pair> onCreateActions;
};

} // namespace parser
} // namespace kuzu
