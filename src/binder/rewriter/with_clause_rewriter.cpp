#include "binder/rewriter/with_clause_rewriter.h"

#include "binder/visitor/property_collector.h"

namespace kuzu {
namespace binder {

void WithClauseRewriter::visitSingleQuery(const NormalizedSingleQuery& singleQuery) {
    auto propertyCollector = std::make_unique<PropertyCollector>();
    auto numQueryPart = (int64_t)singleQuery.getNumQueryParts();

    for (int64_t i = numQueryPart - 2; i >= 0; --i) {
        auto queryPart = singleQuery.getQueryPart(i);
    }
}


}
}