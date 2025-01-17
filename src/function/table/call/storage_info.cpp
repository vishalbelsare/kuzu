#include "binder/binder.h"
#include "common/data_chunk/data_chunk_collection.h"
#include "common/exception/binder.h"
#include "common/type_utils.h"
#include "common/types/interval_t.h"
#include "common/types/ku_string.h"
#include "common/types/types.h"
#include "function/table/bind_input.h"
#include "function/table/simple_table_functions.h"
#include "storage/storage_manager.h"
#include "storage/store/list_chunk_data.h"
#include "storage/store/list_column.h"
#include "storage/store/node_table.h"
#include "storage/store/rel_table.h"
#include "storage/store/string_chunk_data.h"
#include "storage/store/string_column.h"
#include "storage/store/struct_chunk_data.h"
#include "storage/store/struct_column.h"
#include <concepts>

using namespace kuzu::common;
using namespace kuzu::catalog;
using namespace kuzu::storage;
using namespace kuzu::main;

namespace kuzu {
namespace function {

struct StorageInfoLocalState final : TableFuncLocalState {
    std::unique_ptr<DataChunkCollection> dataChunkCollection;
    idx_t currChunkIdx;

    explicit StorageInfoLocalState(MemoryManager* mm) : currChunkIdx{0} {
        dataChunkCollection = std::make_unique<DataChunkCollection>(mm);
    }
};

static void collectColumns(Column* column, std::vector<Column*>& result) {
    result.push_back(column);
    if (column->getNullColumn()) {
        result.push_back(column->getNullColumn());
    }
    switch (column->getDataType().getPhysicalType()) {
    case PhysicalTypeID::STRUCT: {
        const auto structColumn = ku_dynamic_cast<StructColumn*>(column);
        const auto numChildren = StructType::getNumFields(structColumn->getDataType());
        for (auto i = 0u; i < numChildren; i++) {
            const auto childColumn = structColumn->getChild(i);
            collectColumns(childColumn, result);
        }
    } break;
    case PhysicalTypeID::STRING: {
        const auto stringColumn = ku_dynamic_cast<StringColumn*>(column);
        auto& dictionary = stringColumn->getDictionary();
        collectColumns(dictionary.getDataColumn(), result);
        collectColumns(dictionary.getOffsetColumn(), result);
    } break;
    case PhysicalTypeID::ARRAY:
    case PhysicalTypeID::LIST: {
        const auto listColumn = ku_dynamic_cast<ListColumn*>(column);
        collectColumns(listColumn->getOffsetColumn(), result);
        collectColumns(listColumn->getSizeColumn(), result);
        collectColumns(listColumn->getDataColumn(), result);
    } break;
    default: {
        // DO NOTHING.
    }
    }
}

struct StorageInfoBindData final : SimpleTableFuncBindData {
    TableCatalogEntry* tableEntry;
    Table* table;
    const ClientContext* context;

    StorageInfoBindData(binder::expression_vector columns, TableCatalogEntry* tableEntry,
        Table* table, const ClientContext* context)
        : SimpleTableFuncBindData{std::move(columns), 1 /*maxOffset*/}, tableEntry{tableEntry},
          table{table}, context{context} {}

    std::unique_ptr<TableFuncBindData> copy() const override {
        return std::make_unique<StorageInfoBindData>(columns, tableEntry, table, context);
    }
};

static std::unique_ptr<TableFuncLocalState> initLocalState(const TableFunctionInitInput& /*input*/,
    const TableFuncSharedState* /*state*/, MemoryManager* mm) {
    return std::make_unique<StorageInfoLocalState>(mm);
}

struct StorageInfoOutputData {
    node_group_idx_t nodeGroupIdx = INVALID_NODE_GROUP_IDX;
    node_group_idx_t chunkIdx = INVALID_NODE_GROUP_IDX;
    std::string tableType;
    uint32_t columnIdx = INVALID_COLUMN_ID;
    std::vector<Column*> columns;
};

static void resetOutputIfNecessary(const StorageInfoLocalState* localState,
    DataChunk& outputChunk) {
    if (outputChunk.state->getSelVector().getSelSize() == DEFAULT_VECTOR_CAPACITY) {
        localState->dataChunkCollection->append(outputChunk);
        outputChunk.resetAuxiliaryBuffer();
        outputChunk.state->getSelVectorUnsafe().setSelSize(0);
    }
}

static void appendStorageInfoForChunkData(StorageInfoLocalState* localState, DataChunk& outputChunk,
    StorageInfoOutputData& outputData, ColumnChunkData& chunkData, bool ignoreNull = false) {
    resetOutputIfNecessary(localState, outputChunk);
    auto vectorPos = outputChunk.state->getSelVector().getSelSize();
    auto residency = chunkData.getResidencyState();
    ColumnChunkMetadata metadata;
    switch (residency) {
    case ResidencyState::IN_MEMORY: {
        metadata = chunkData.getMetadataToFlush();
    } break;
    case ResidencyState::ON_DISK: {
        metadata = chunkData.getMetadata();
    } break;
    default: {
        KU_UNREACHABLE;
    }
    }
    auto& columnType = chunkData.getDataType();
    outputChunk.getValueVectorMutable(0).setValue(vectorPos, outputData.tableType);
    outputChunk.getValueVectorMutable(1).setValue<uint64_t>(vectorPos, outputData.nodeGroupIdx);
    outputChunk.getValueVectorMutable(2).setValue<uint64_t>(vectorPos, outputData.chunkIdx);
    outputChunk.getValueVectorMutable(3).setValue(vectorPos,
        ResidencyStateUtils::toString(residency));
    outputChunk.getValueVectorMutable(4).setValue(vectorPos,
        outputData.columns[outputData.columnIdx++]->getName());
    outputChunk.getValueVectorMutable(5).setValue(vectorPos, columnType.toString());
    outputChunk.getValueVectorMutable(6).setValue<uint64_t>(vectorPos, metadata.pageIdx);
    outputChunk.getValueVectorMutable(7).setValue<uint64_t>(vectorPos, metadata.numPages);
    outputChunk.getValueVectorMutable(8).setValue<uint64_t>(vectorPos, metadata.numValues);

    auto customToString = [&]<typename T>(T) {
        outputChunk.getValueVectorMutable(9).setValue(vectorPos,
            std::to_string(metadata.compMeta.min.get<T>()));
        outputChunk.getValueVectorMutable(10).setValue(vectorPos,
            std::to_string(metadata.compMeta.max.get<T>()));
    };
    auto physicalType = columnType.getPhysicalType();
    TypeUtils::visit(
        physicalType, [&](ku_string_t) { customToString(uint32_t()); },
        [&](list_entry_t) { customToString(uint64_t()); },
        [&](internalID_t) { customToString(uint64_t()); },
        [&]<typename T>(T)
            requires(std::integral<T> || std::floating_point<T>)
        {
            auto min = metadata.compMeta.min.get<T>();
            auto max = metadata.compMeta.max.get<T>();
            outputChunk.getValueVectorMutable(9).setValue(vectorPos,
                TypeUtils::entryToString(columnType, (uint8_t*)&min,
                    &outputChunk.getValueVectorMutable(9)));
            outputChunk.getValueVectorMutable(10).setValue(vectorPos,
                TypeUtils::entryToString(columnType, (uint8_t*)&max,
                    &outputChunk.getValueVectorMutable(10)));
        },
        // Types which don't support statistics.
        // types not supported by TypeUtils::visit can
        // also be ignored since we don't track statistics for them
        [](int128_t) {}, [](struct_entry_t) {}, [](interval_t) {});
    outputChunk.getValueVectorMutable(11).setValue(vectorPos,
        metadata.compMeta.toString(physicalType));
    outputChunk.state->getSelVectorUnsafe().incrementSelSize();
    if (columnType.getPhysicalType() == PhysicalTypeID::INTERNAL_ID) {
        ignoreNull = true;
    }
    if (!ignoreNull && chunkData.hasNullData()) {
        appendStorageInfoForChunkData(localState, outputChunk, outputData,
            *chunkData.getNullData());
    }
    switch (columnType.getPhysicalType()) {
    case PhysicalTypeID::STRUCT: {
        auto& structChunk = chunkData.cast<StructChunkData>();
        auto numChildren = structChunk.getNumChildren();
        for (auto i = 0u; i < numChildren; i++) {
            appendStorageInfoForChunkData(localState, outputChunk, outputData,
                *structChunk.getChild(i));
        }
    } break;
    case PhysicalTypeID::STRING: {
        auto& stringChunk = chunkData.cast<StringChunkData>();
        auto& dictionaryChunk = stringChunk.getDictionaryChunk();
        appendStorageInfoForChunkData(localState, outputChunk, outputData,
            *dictionaryChunk.getStringDataChunk());
        appendStorageInfoForChunkData(localState, outputChunk, outputData,
            *dictionaryChunk.getOffsetChunk());
    } break;
    case PhysicalTypeID::ARRAY:
    case PhysicalTypeID::LIST: {
        auto& listChunk = chunkData.cast<ListChunkData>();
        appendStorageInfoForChunkData(localState, outputChunk, outputData,
            *listChunk.getOffsetColumnChunk());
        appendStorageInfoForChunkData(localState, outputChunk, outputData,
            *listChunk.getSizeColumnChunk());
        appendStorageInfoForChunkData(localState, outputChunk, outputData,
            *listChunk.getDataColumnChunk());
    } break;
    default: {
        // DO NOTHING.
    }
    }
}

static void appendStorageInfoForChunkedGroup(StorageInfoLocalState* localState,
    DataChunk& outputChunk, StorageInfoOutputData& outputData, ChunkedNodeGroup* chunkedGroup) {
    auto numColumns = chunkedGroup->getNumColumns();
    outputData.columnIdx = 0;
    for (auto i = 0u; i < numColumns; i++) {
        resetOutputIfNecessary(localState, outputChunk);
        appendStorageInfoForChunkData(localState, outputChunk, outputData,
            chunkedGroup->getColumnChunk(i).getData());
    }
    if (chunkedGroup->getFormat() == NodeGroupDataFormat::CSR) {
        auto& chunkedCSRGroup = chunkedGroup->cast<ChunkedCSRNodeGroup>();
        resetOutputIfNecessary(localState, outputChunk);
        appendStorageInfoForChunkData(localState, outputChunk, outputData,
            chunkedCSRGroup.getCSRHeader().length->getData(), true);
        resetOutputIfNecessary(localState, outputChunk);
        appendStorageInfoForChunkData(localState, outputChunk, outputData,
            chunkedCSRGroup.getCSRHeader().length->getData(), true);
    }
}

static void appendStorageInfoForNodeGroup(StorageInfoLocalState* localState, DataChunk& outputChunk,
    StorageInfoOutputData& outputData, NodeGroup* nodeGroup) {
    auto numChunks = nodeGroup->getNumChunkedGroups();
    for (auto chunkIdx = 0ul; chunkIdx < numChunks; chunkIdx++) {
        outputData.chunkIdx = chunkIdx;
        appendStorageInfoForChunkedGroup(localState, outputChunk, outputData,
            nodeGroup->getChunkedNodeGroup(chunkIdx));
    }
    if (nodeGroup->getFormat() == NodeGroupDataFormat::CSR) {
        auto& csrNodeGroup = nodeGroup->cast<CSRNodeGroup>();
        auto persistentChunk = csrNodeGroup.getPersistentChunkedGroup();
        if (persistentChunk) {
            outputData.chunkIdx = INVALID_NODE_GROUP_IDX;
            appendStorageInfoForChunkedGroup(localState, outputChunk, outputData,
                csrNodeGroup.getPersistentChunkedGroup());
        }
    }
}

static offset_t tableFunc(const TableFuncInput& input, TableFuncOutput& output) {
    auto& dataChunk = output.dataChunk;
    auto localState = ku_dynamic_cast<StorageInfoLocalState*>(input.localState);
    KU_ASSERT(dataChunk.state->getSelVector().isUnfiltered());
    while (true) {
        if (localState->currChunkIdx < localState->dataChunkCollection->getNumChunks()) {
            // Copy from local state chunk.
            const auto& chunk =
                localState->dataChunkCollection->getChunkUnsafe(localState->currChunkIdx);
            const auto numValuesToOutput = chunk.state->getSelVector().getSelSize();
            for (auto columnIdx = 0u; columnIdx < dataChunk.getNumValueVectors(); columnIdx++) {
                const auto& localVector = chunk.getValueVector(columnIdx);
                auto& outputVector = dataChunk.getValueVectorMutable(columnIdx);
                for (auto i = 0u; i < numValuesToOutput; i++) {
                    outputVector.copyFromVectorData(i, &localVector, i);
                }
            }
            dataChunk.state->getSelVectorUnsafe().setToUnfiltered(numValuesToOutput);
            localState->currChunkIdx++;
            return numValuesToOutput;
        }
        auto morsel = input.sharedState->ptrCast<SimpleTableFuncSharedState>()->getMorsel();
        if (!morsel.hasMoreToOutput()) {
            return 0;
        }
        const auto bindData = input.bindData->constPtrCast<StorageInfoBindData>();
        const auto table = bindData->table;
        StorageInfoOutputData outputData;
        outputData.tableType = table->getTableType() == TableType::NODE ? "NODE" : "REL";
        node_group_idx_t numNodeGroups = 0;
        switch (table->getTableType()) {
        case TableType::NODE: {
            auto& nodeTable = table->cast<NodeTable>();
            std::vector<Column*> columns;
            for (auto columnID = 0u; columnID < nodeTable.getNumColumns(); columnID++) {
                collectColumns(&nodeTable.getColumn(columnID), columns);
            }
            outputData.columns = std::move(columns);
            numNodeGroups = nodeTable.getNumNodeGroups();
            for (auto i = 0ul; i < numNodeGroups; i++) {
                outputData.nodeGroupIdx = i;
                appendStorageInfoForNodeGroup(localState, dataChunk, outputData,
                    nodeTable.getNodeGroup(i));
            }
        } break;
        case TableType::REL: {
            auto& relTable = table->cast<RelTable>();
            auto appendDirectedStorageInfo = [&](RelDataDirection direction) {
                auto directedRelTableData = relTable.getDirectedTableData(direction);
                std::vector<Column*> columns;
                for (auto columnID = 0u; columnID < relTable.getNumColumns(); columnID++) {
                    collectColumns(directedRelTableData->getColumn(columnID), columns);
                }
                columns.push_back(directedRelTableData->getCSROffsetColumn());
                columns.push_back(directedRelTableData->getCSRLengthColumn());
                outputData.columns = std::move(columns);
                numNodeGroups = directedRelTableData->getNumNodeGroups();
                for (auto i = 0ul; i < numNodeGroups; i++) {
                    outputData.nodeGroupIdx = i;
                    appendStorageInfoForNodeGroup(localState, dataChunk, outputData,
                        directedRelTableData->getNodeGroup(i));
                }
            };
            for (auto direction : relTable.getStorageDirections()) {
                appendDirectedStorageInfo(direction);
            }
        } break;
        default: {
            KU_UNREACHABLE;
        }
        }
        localState->dataChunkCollection->append(dataChunk);
        dataChunk.resetAuxiliaryBuffer();
        dataChunk.state->getSelVectorUnsafe().setSelSize(0);
    }
}

static std::unique_ptr<TableFuncBindData> bindFunc(const ClientContext* context,
    const TableFuncBindInput* input) {
    const std::vector<std::string> columnNames = {"table_type", "node_group_id", "node_chunk_id",
        "residency", "column_name", "data_type", "start_page_idx", "num_pages", "num_values", "min",
        "max", "compression"};
    std::vector<LogicalType> columnTypes;
    columnTypes.emplace_back(LogicalType::STRING());
    columnTypes.emplace_back(LogicalType::INT64());
    columnTypes.emplace_back(LogicalType::INT64());
    columnTypes.emplace_back(LogicalType::STRING());
    columnTypes.emplace_back(LogicalType::STRING());
    columnTypes.emplace_back(LogicalType::STRING());
    columnTypes.emplace_back(LogicalType::INT64());
    columnTypes.emplace_back(LogicalType::INT64());
    columnTypes.emplace_back(LogicalType::INT64());
    columnTypes.emplace_back(LogicalType::STRING());
    columnTypes.emplace_back(LogicalType::STRING());
    columnTypes.emplace_back(LogicalType::STRING());
    auto tableName = input->getLiteralVal<std::string>(0);
    auto catalog = context->getCatalog();
    if (!catalog->containsTable(context->getTransaction(), tableName)) {
        throw BinderException{"Table " + tableName + " does not exist!"};
    }
    auto tableEntry = catalog->getTableCatalogEntry(context->getTransaction(), tableName);
    auto storageManager = context->getStorageManager();
    auto table = storageManager->getTable(tableEntry->getTableID());
    auto columns = input->binder->createVariables(columnNames, columnTypes);
    return std::make_unique<StorageInfoBindData>(columns, tableEntry, table, context);
}

function_set StorageInfoFunction::getFunctionSet() {
    function_set functionSet;
    auto function = std::make_unique<TableFunction>(name, std::vector{LogicalTypeID::STRING});
    function->tableFunc = tableFunc;
    function->bindFunc = bindFunc;
    function->initSharedStateFunc = initSharedState;
    function->initLocalStateFunc = initLocalState;
    functionSet.push_back(std::move(function));
    return functionSet;
}

} // namespace function
} // namespace kuzu
