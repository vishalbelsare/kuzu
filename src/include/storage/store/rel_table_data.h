#pragma once

#include <cmath>

#include "common/enums/rel_direction.h"
#include "common/enums/rel_multiplicity.h"
#include "storage/store/column.h"
#include "storage/store/csr_node_group.h"
#include "storage/store/node_group_collection.h"

namespace kuzu {
namespace transaction {
class Transaction;
}
namespace storage {
class MemoryManager;
class RelTableData;

struct CSRHeaderColumns {
    std::unique_ptr<Column> offset;
    std::unique_ptr<Column> length;
};

class PersistentVersionRecordHandler : public VersionRecordHandler {
public:
    explicit PersistentVersionRecordHandler(RelTableData* relTableData);

    void applyFuncToChunkedGroups(version_record_handler_op_t func,
        common::node_group_idx_t nodeGroupIdx, common::row_idx_t startRow,
        common::row_idx_t numRows, common::transaction_t commitTS) const override;
    void rollbackInsert(const transaction::Transaction* transaction,
        common::node_group_idx_t nodeGroupIdx, common::row_idx_t startRow,
        common::row_idx_t numRows) const override;

private:
    RelTableData* relTableData;
};

class InMemoryVersionRecordHandler : public VersionRecordHandler {
public:
    explicit InMemoryVersionRecordHandler(RelTableData* relTableData);

    void applyFuncToChunkedGroups(version_record_handler_op_t func,
        common::node_group_idx_t nodeGroupIdx, common::row_idx_t startRow,
        common::row_idx_t numRows, common::transaction_t commitTS) const override;
    void rollbackInsert(const transaction::Transaction* transaction,
        common::node_group_idx_t nodeGroupIdx, common::row_idx_t startRow,
        common::row_idx_t numRows) const override;

private:
    RelTableData* relTableData;
};

class RelTableData {
public:
    RelTableData(FileHandle* dataFH, MemoryManager* mm, ShadowFile* shadowFile,
        const catalog::TableCatalogEntry* tableEntry, common::RelDataDirection direction,
        bool enableCompression, common::Deserializer* deSer);

    bool update(transaction::Transaction* transaction, common::ValueVector& boundNodeIDVector,
        const common::ValueVector& relIDVector, common::column_id_t columnID,
        const common::ValueVector& dataVector) const;
    bool delete_(transaction::Transaction* transaction, common::ValueVector& boundNodeIDVector,
        const common::ValueVector& relIDVector);
    void addColumn(transaction::Transaction* transaction, TableAddColumnState& addColumnState);

    bool checkIfNodeHasRels(transaction::Transaction* transaction,
        common::ValueVector* srcNodeIDVector) const;

    Column* getNbrIDColumn() const { return columns[NBR_ID_COLUMN_ID].get(); }
    Column* getCSROffsetColumn() const { return csrHeaderColumns.offset.get(); }
    Column* getCSRLengthColumn() const { return csrHeaderColumns.length.get(); }
    common::column_id_t getNumColumns() const { return columns.size(); }
    Column* getColumn(common::column_id_t columnID) const { return columns[columnID].get(); }
    std::vector<const Column*> getColumns() const {
        std::vector<const Column*> result;
        result.reserve(columns.size());
        for (const auto& column : columns) {
            result.push_back(column.get());
        }
        return result;
    }
    common::node_group_idx_t getNumNodeGroups() const { return nodeGroups->getNumNodeGroups(); }
    NodeGroup* getNodeGroup(common::node_group_idx_t nodeGroupIdx) const {
        return nodeGroups->getNodeGroup(nodeGroupIdx, true /*mayOutOfBound*/);
    }
    NodeGroup* getOrCreateNodeGroup(transaction::Transaction* transaction,
        common::node_group_idx_t nodeGroupIdx) const {
        return nodeGroups->getOrCreateNodeGroup(transaction, nodeGroupIdx,
            NodeGroupDataFormat::CSR);
    }

    common::RelMultiplicity getMultiplicity() const { return multiplicity; }

    TableStats getStats() const { return nodeGroups->getStats(); }

    void checkpoint(const std::vector<common::column_id_t>& columnIDs);

    void pushInsertInfo(transaction::Transaction* transaction, const CSRNodeGroup& nodeGroup,
        common::row_idx_t numRows_, CSRNodeGroupScanSource source);

    void serialize(common::Serializer& serializer) const;

    NodeGroup* getNodeGroupNoLock(common::node_group_idx_t nodeGroupIdx) const {
        return nodeGroups->getNodeGroupNoLock(nodeGroupIdx);
    }

    void rollbackGroupCollectionInsert(common::row_idx_t numRows_, bool isPersistent);

    common::RelDataDirection getDirection() const { return direction; }

private:
    void initCSRHeaderColumns();
    void initPropertyColumns(const catalog::TableCatalogEntry* tableEntry);

    std::pair<CSRNodeGroupScanSource, common::row_idx_t> findMatchingRow(
        transaction::Transaction* transaction, common::ValueVector& boundNodeIDVector,
        const common::ValueVector& relIDVector) const;

    template<typename T1, typename T2>
    static double divideNoRoundUp(T1 v1, T2 v2) {
        static_assert(std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>);
        return static_cast<double>(v1) / static_cast<double>(v2);
    }
    template<typename T1, typename T2>
    static uint64_t multiplyAndRoundUpTo(T1 v1, T2 v2) {
        static_assert(std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>);
        return std::ceil(static_cast<double>(v1) * static_cast<double>(v2));
    }

    std::vector<common::LogicalType> getColumnTypes() const {
        std::vector<common::LogicalType> types;
        types.reserve(columns.size());
        for (const auto& column : columns) {
            types.push_back(column->getDataType().copy());
        }
        return types;
    }

    const VersionRecordHandler* getVersionRecordHandler(CSRNodeGroupScanSource source);

private:
    FileHandle* dataFH;
    common::table_id_t tableID;
    std::string tableName;
    MemoryManager* memoryManager;
    ShadowFile* shadowFile;
    bool enableCompression;
    PackedCSRInfo packedCSRInfo;
    common::RelDataDirection direction;
    common::RelMultiplicity multiplicity;

    std::unique_ptr<NodeGroupCollection> nodeGroups;

    CSRHeaderColumns csrHeaderColumns;
    std::vector<std::unique_ptr<Column>> columns;

    PersistentVersionRecordHandler persistentVersionRecordHandler;
    InMemoryVersionRecordHandler inMemoryVersionRecordHandler;
};

} // namespace storage
} // namespace kuzu
