#pragma once

#include "binder/expression/node_expression.h"
#include "function/table/bind_data.h"
#include "function/table/table_function.h"
#include "storage/index/hnsw_index.h"

namespace kuzu {
namespace function {

struct CreateHNSWIndexBindData final : TableFuncBindData {
    main::ClientContext* context;
    std::string indexName;
    catalog::TableCatalogEntry* tableEntry;
    common::property_id_t propertyID;
    storage::HNSWIndexConfig config;
    common::offset_t numNodes;

    CreateHNSWIndexBindData(main::ClientContext* context, std::string indexName,
        catalog::TableCatalogEntry* tableEntry, common::property_id_t propertyID,
        common::offset_t numNodes, common::offset_t maxOffset, storage::HNSWIndexConfig config)
        : TableFuncBindData{maxOffset}, context{context}, indexName{std::move(indexName)},
          tableEntry{tableEntry}, propertyID{propertyID}, config{std::move(config)},
          numNodes{numNodes} {}

    std::unique_ptr<TableFuncBindData> copy() const override {
        return std::make_unique<CreateHNSWIndexBindData>(context, indexName, tableEntry, propertyID,
            numNodes, maxOffset, config.copy());
    }
};

struct CreateHNSWSharedState final : TableFuncSharedState {
    std::string name;
    std::unique_ptr<storage::InMemHNSWIndex> hnswIndex;
    storage::NodeTable& nodeTable;
    common::offset_t numNodes;
    std::atomic<common::offset_t> numNodesInserted = 0;

    const CreateHNSWIndexBindData* bindData;
    std::shared_ptr<storage::HNSWIndexPartitionerSharedState> partitionerSharedState;

    explicit CreateHNSWSharedState(const CreateHNSWIndexBindData& bindData);
};

struct CreateHNSWLocalState final : TableFuncLocalState {
    storage::VisitedState upperVisited;
    storage::VisitedState lowerVisited;

    explicit CreateHNSWLocalState(common::offset_t numNodes)
        : upperVisited{numNodes}, lowerVisited{numNodes} {}
};

struct BoundQueryHNSWIndexInput {
    catalog::NodeTableCatalogEntry* nodeTableEntry;
    catalog::IndexCatalogEntry* indexEntry;
    std::shared_ptr<binder::Expression> queryExpression;
    uint64_t k;
};

struct QueryHNSWIndexBindData final : TableFuncBindData {
    main::ClientContext* context;
    BoundQueryHNSWIndexInput boundInput;
    common::column_id_t indexColumnID;
    catalog::RelTableCatalogEntry* upperHNSWRelTableEntry;
    catalog::RelTableCatalogEntry* lowerHNSWRelTableEntry;
    storage::QueryHNSWConfig config;

    std::shared_ptr<binder::NodeExpression> outputNode;

    QueryHNSWIndexBindData(main::ClientContext* context, binder::expression_vector columns,
        BoundQueryHNSWIndexInput boundInput, storage::QueryHNSWConfig config,
        std::shared_ptr<binder::NodeExpression> outputNode);

    std::shared_ptr<binder::Expression> getNodeOutput() const override { return outputNode; }

    std::unique_ptr<TableFuncBindData> copy() const override {
        return std::make_unique<QueryHNSWIndexBindData>(context, columns, boundInput, config,
            outputNode);
    }
};

struct QueryHNSWIndexSharedState final : TableFuncSharedState {
    storage::NodeTable* nodeTable;
    common::offset_t numNodes;

    explicit QueryHNSWIndexSharedState(const QueryHNSWIndexBindData& bindData);
};

struct QueryHNSWLocalState final : TableFuncLocalState {
    storage::VisitedState visited;
    std::optional<std::vector<storage::NodeWithDistance>> result;
    uint64_t numRowsOutput = 0;
    storage::OnDiskEmbeddingScanState embeddingScanState;

    QueryHNSWLocalState(storage::MemoryManager* mm, storage::NodeTable& nodeTable,
        common::column_id_t columnID, common::offset_t numNodes)
        : visited{numNodes}, embeddingScanState{mm, nodeTable, columnID} {}

    bool hasResultToOutput() const { return result.has_value(); }
};

struct CreateHNSWIndexFunction final {
    static constexpr const char* name = "CREATE_HNSW_INDEX";

    static function_set getFunctionSet();
};

struct DropHNSWIndexFunction final {
    static constexpr const char* name = "DROP_HNSW_INDEX";

    static function_set getFunctionSet();
};

struct QueryHNSWIndexFunction final {
    static constexpr const char* name = "QUERY_HNSW_INDEX";

    static function_set getFunctionSet();
};

} // namespace function
} // namespace kuzu
