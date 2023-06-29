#include "binder/bound_statement_visitor.h"

namespace kuzu {
namespace binder {

class WithClauseRewriter : public BoundStatementVisitor {
public:
    WithClauseRewriter() = default;

private:
    void visitSingleQuery(const NormalizedSingleQuery &singleQuery) override;
};

}
}