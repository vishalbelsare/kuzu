#pragma once

#include "common/copier_config/copier_config.h"
#include "processor/operator/sink.h"
#include "storage/store/node_group.h"
#include "storage/store/node_table.h"

namespace kuzu {
namespace processor {

class CopyNodeSharedState {
public:
    CopyNodeSharedState(uint64_t& numRows, catalog::NodeTableSchema* tableSchema,
        storage::NodeTable* table, const common::CopyDescription& copyDesc,
        storage::MemoryManager* memoryManager);

    inline void initialize(const std::string& directory) { initializePrimaryKey(directory); };

    inline common::offset_t getNextNodeGroupIdx() {
        std::unique_lock<std::mutex> lck{mtx};
        return getNextNodeGroupIdxWithoutLock();
    }

    void logCopyNodeWALRecord(storage::WAL* wal);

    void appendLocalNodeGroup(std::unique_ptr<storage::NodeGroup> localNodeGroup);

private:
    void initializePrimaryKey(const std::string& directory);
    inline common::offset_t getNextNodeGroupIdxWithoutLock() { return currentNodeGroupIdx++; }

public:
    std::mutex mtx;
    common::column_id_t pkColumnID;
    std::unique_ptr<storage::PrimaryKeyIndexBuilder> pkIndex;
    common::CopyDescription copyDesc;
    storage::NodeTable* table;
    catalog::NodeTableSchema* tableSchema;
    uint64_t& numRows;
    std::shared_ptr<FactorizedTable> fTable;
    bool hasLoggedWAL;
    uint64_t currentNodeGroupIdx;
    // The sharedNodeGroup is to accumulate left data within local node groups in CopyNode ops.
    std::unique_ptr<storage::NodeGroup> sharedNodeGroup;
};

struct CopyNodeInfo {
    DataPos rowIdxVectorPos;
    DataPos filePathVectorPos;
    std::vector<DataPos> dataColumnPoses;
    common::CopyDescription copyDesc;
    storage::NodeTable* table;
    storage::RelsStore* relsStore;
    catalog::Catalog* catalog;
    storage::WAL* wal;
};

class CopyNode : public Sink {
public:
    CopyNode(std::shared_ptr<CopyNodeSharedState> sharedState, CopyNodeInfo copyNodeInfo,
        std::unique_ptr<ResultSetDescriptor> resultSetDescriptor,
        std::unique_ptr<PhysicalOperator> child, uint32_t id, const std::string& paramsString);

    inline void initLocalStateInternal(ResultSet* resultSet, ExecutionContext* context) override {
        rowIdxVector = resultSet->getValueVector(copyNodeInfo.rowIdxVectorPos).get();
        filePathVector = resultSet->getValueVector(copyNodeInfo.filePathVectorPos).get();
        for (auto& arrowColumnPos : copyNodeInfo.dataColumnPoses) {
            dataColumnVectors.push_back(resultSet->getValueVector(arrowColumnPos).get());
        }
        localNodeGroup =
            std::make_unique<storage::NodeGroup>(sharedState->tableSchema, &sharedState->copyDesc);
    }

    inline void initGlobalStateInternal(ExecutionContext* context) override;

    void executeInternal(ExecutionContext* context) override;

    void finalize(ExecutionContext* context) override;

    inline std::unique_ptr<PhysicalOperator> clone() override {
        return std::make_unique<CopyNode>(sharedState, copyNodeInfo, resultSetDescriptor->copy(),
            children[0]->clone(), id, paramsString);
    }

    static void appendNodeGroupToTableAndPopulateIndex(storage::NodeTable* table,
        storage::NodeGroup* nodeGroup, storage::PrimaryKeyIndexBuilder* pkIndex,
        common::column_id_t pkColumnID);

private:
    inline bool isCopyAllowed() const {
        auto nodesStatistics = copyNodeInfo.table->getNodeStatisticsAndDeletedIDs();
        return nodesStatistics->getNodeStatisticsAndDeletedIDs(copyNodeInfo.table->getTableID())
                   ->getNumTuples() == 0;
    }

    static std::shared_ptr<common::DataChunk> sliceDataVectorsInDataChunk(
        const common::DataChunk& dataChunkToSlice, const std::vector<DataPos>& dataColumnPoses,
        int64_t offset, int64_t length);

    static void populatePKIndex(storage::PrimaryKeyIndexBuilder* pkIndex,
        storage::ColumnChunk* chunk, common::offset_t startNodeOffset, common::offset_t numNodes);
    static void appendToPKIndex(storage::PrimaryKeyIndexBuilder* pkIndex,
        storage::ColumnChunk* chunk, common::offset_t startOffset, common::offset_t numNodes);

private:
    std::shared_ptr<CopyNodeSharedState> sharedState;
    CopyNodeInfo copyNodeInfo;
    common::ValueVector* rowIdxVector;
    common::ValueVector* filePathVector;
    std::vector<common::ValueVector*> dataColumnVectors;
    std::unique_ptr<storage::NodeGroup> localNodeGroup;
};

} // namespace processor
} // namespace kuzu
