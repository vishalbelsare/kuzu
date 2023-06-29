#include "bound_statement.h"

namespace kuzu {
namespace binder {

class BoundStatementRewriter {
public:
    void rewrite(BoundStatement& boundStatement);
};

}
}