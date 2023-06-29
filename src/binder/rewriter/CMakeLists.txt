add_library(
        kuzu_binder_rewriter
        OBJECT
        with_clause_rewriter.cpp)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:kuzu_binder_rewriter>
        PARENT_SCOPE)