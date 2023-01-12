#pragma once

#include "common/types/types_include.h"
#include "common/utils.h"

using namespace kuzu::common;

namespace kuzu {
namespace storage {

enum class ListType : uint8_t {
    ADJ_LISTS = 0,
    REL_PROPERTY_LISTS = 1,
};

enum class ListFileType : uint8_t {
    BASE_LISTS = 0,
    HEADERS = 1,
    METADATA = 2,
};

enum class ColumnType : uint8_t {
    NODE_PROPERTY_COLUMN = 0,
    ADJ_COLUMN = 1,
    REL_PROPERTY_COLUMN = 2,
};

struct RelNodeTableAndDir {
    table_id_t relTableID;
    table_id_t srcNodeTableID;
    RelDirection dir;

    RelNodeTableAndDir() = default;

    RelNodeTableAndDir(table_id_t relTableID, table_id_t srcNodeTableID, RelDirection dir)
        : relTableID{relTableID}, srcNodeTableID{srcNodeTableID}, dir{dir} {}

    inline bool operator==(const RelNodeTableAndDir& rhs) const {
        return relTableID == rhs.relTableID && srcNodeTableID == rhs.srcNodeTableID &&
               dir == rhs.dir;
    }
};

struct AdjListsID {
    RelNodeTableAndDir relNodeTableAndDir;

    AdjListsID() = default;

    explicit AdjListsID(RelNodeTableAndDir relNodeTableAndDir)
        : relNodeTableAndDir{relNodeTableAndDir} {}

    inline bool operator==(const AdjListsID& rhs) const {
        return relNodeTableAndDir == rhs.relNodeTableAndDir;
    }
};

struct RelPropertyListID {
    RelNodeTableAndDir relNodeTableAndDir;
    property_id_t propertyID;

    RelPropertyListID() = default;

    RelPropertyListID(RelNodeTableAndDir relNodeTableAndDir, property_id_t propertyID)
        : relNodeTableAndDir{relNodeTableAndDir}, propertyID{propertyID} {}

    inline bool operator==(const RelPropertyListID& rhs) const {
        return relNodeTableAndDir == rhs.relNodeTableAndDir && propertyID == rhs.propertyID;
    }
};

struct ListFileID {
    ListType listType;
    ListFileType listFileType;
    union {
        AdjListsID adjListsID;
        RelPropertyListID relPropertyListID;
    };

    ListFileID() = default;

    ListFileID(ListFileType listFileType, AdjListsID adjListsID)
        : listType{ListType::ADJ_LISTS}, listFileType{listFileType}, adjListsID{adjListsID} {}

    ListFileID(ListFileType listFileType, RelPropertyListID relPropertyListID)
        : listType{ListType::REL_PROPERTY_LISTS}, listFileType{listFileType},
          relPropertyListID{relPropertyListID} {}

    inline bool operator==(const ListFileID& rhs) const {
        if (listType != rhs.listType || listFileType != rhs.listFileType) {
            return false;
        }
        switch (listType) {
        case ListType::ADJ_LISTS: {
            return adjListsID == rhs.adjListsID;
        }
        case ListType::REL_PROPERTY_LISTS: {
            return relPropertyListID == rhs.relPropertyListID;
        }
        }
    }
};

struct NodePropertyColumnID {
    table_id_t tableID;
    property_id_t propertyID;

    NodePropertyColumnID() = default;

    NodePropertyColumnID(table_id_t tableID, property_id_t propertyID)
        : tableID{tableID}, propertyID{propertyID} {}

    inline bool operator==(const NodePropertyColumnID& rhs) const {
        return tableID == rhs.tableID && propertyID == rhs.propertyID;
    }
};

struct AdjColumnID {
    RelNodeTableAndDir relNodeTableAndDir;

    AdjColumnID() = default;

    explicit AdjColumnID(RelNodeTableAndDir relNodeTableAndDir)
        : relNodeTableAndDir{relNodeTableAndDir} {}

    inline bool operator==(const AdjColumnID& rhs) const {
        return relNodeTableAndDir == rhs.relNodeTableAndDir;
    }
};

struct RelPropertyColumnID {
    RelNodeTableAndDir relNodeTableAndDir;
    property_id_t propertyID;

    RelPropertyColumnID() = default;

    RelPropertyColumnID(RelNodeTableAndDir relNodeTableAndDir, property_id_t propertyID)
        : relNodeTableAndDir{relNodeTableAndDir}, propertyID{propertyID} {}

    inline bool operator==(const RelPropertyColumnID& rhs) const {
        return relNodeTableAndDir == rhs.relNodeTableAndDir && propertyID == rhs.propertyID;
    }
};

struct ColumnFileID {
    ColumnType columnType;
    union {
        NodePropertyColumnID nodePropertyColumnID;
        AdjColumnID adjColumnID;
        RelPropertyColumnID relPropertyColumnID;
    };

    ColumnFileID() = default;

    explicit ColumnFileID(NodePropertyColumnID nodePropertyColumnID)
        : columnType{ColumnType::NODE_PROPERTY_COLUMN}, nodePropertyColumnID{nodePropertyColumnID} {
    }

    explicit ColumnFileID(AdjColumnID adjColumnID)
        : columnType{ColumnType::ADJ_COLUMN}, adjColumnID{adjColumnID} {}

    explicit ColumnFileID(RelPropertyColumnID relPropertyColumnID)
        : columnType{ColumnType::REL_PROPERTY_COLUMN}, relPropertyColumnID{relPropertyColumnID} {}

    inline bool operator==(const ColumnFileID& rhs) const {
        if (columnType != rhs.columnType) {
            return false;
        }
        switch (columnType) {
        case ColumnType::NODE_PROPERTY_COLUMN: {
            return nodePropertyColumnID == rhs.nodePropertyColumnID;
        }
        case ColumnType::ADJ_COLUMN: {
            return adjColumnID == rhs.adjColumnID;
        }
        case ColumnType::REL_PROPERTY_COLUMN: {
            return relPropertyColumnID == rhs.relPropertyColumnID;
        }
        default: {
            assert(false);
        }
        }
    }
};

struct NodeIndexID {
    table_id_t tableID;

    NodeIndexID() = default;

    explicit NodeIndexID(table_id_t tableID) : tableID{tableID} {}

    inline bool operator==(const NodeIndexID& rhs) const { return tableID == rhs.tableID; }
};

enum class StorageStructureType : uint8_t {
    COLUMN = 0,
    LISTS = 1,
    NODE_INDEX = 2,
};

string storageStructureTypeToString(StorageStructureType storageStructureType);

// StorageStructureIDs start with 1 byte type and 1 byte isOverflow field followed with additional
// bytes needed by the different log types. We don't need these to be byte aligned because they are
// not stored in memory. These are used to serialize and deserialize log entries.
struct StorageStructureID {
    StorageStructureType storageStructureType;
    bool isOverflow;
    union {
        ColumnFileID columnFileID;
        ListFileID listFileID;
        NodeIndexID nodeIndexID;
    };

    inline bool operator==(const StorageStructureID& rhs) const {
        if (storageStructureType != rhs.storageStructureType || isOverflow != rhs.isOverflow) {
            return false;
        }
        switch (storageStructureType) {
        case StorageStructureType::COLUMN: {
            return columnFileID == rhs.columnFileID;
        }
        case StorageStructureType::LISTS: {
            return listFileID == rhs.listFileID;
        }
        case StorageStructureType::NODE_INDEX: {
            return nodeIndexID == rhs.nodeIndexID;
        }
        default: {
            assert(false);
        }
        }
    }

    static StorageStructureID newNodePropertyColumnID(table_id_t tableID, property_id_t propertyID);

    static StorageStructureID newRelPropertyColumnID(
        table_id_t nodeTableID, table_id_t relTableID, RelDirection dir, property_id_t propertyID);

    static StorageStructureID newAdjColumnID(
        table_id_t nodeTableID, table_id_t relTableID, RelDirection dir);

    static StorageStructureID newNodeIndexID(table_id_t tableID);

    static StorageStructureID newAdjListsID(
        table_id_t tableID, table_id_t srcNodeTableID, RelDirection dir, ListFileType listFileType);

    static StorageStructureID newRelPropertyListsID(table_id_t nodeTableID, table_id_t relTableID,
        RelDirection dir, property_id_t propertyID, ListFileType listFileType);
};

enum class WALRecordType : uint8_t {
    PAGE_UPDATE_OR_INSERT_RECORD = 0,
    TABLE_STATISTICS_RECORD = 1,
    COMMIT_RECORD = 2,
    CATALOG_RECORD = 3,
    NODE_TABLE_RECORD = 4,
    REL_TABLE_RECORD = 5,
    // Records the nextBytePosToWriteTo field's last value before the write trx started. This is
    // used when rolling back to restore this value.
    OVERFLOW_FILE_NEXT_BYTE_POS_RECORD = 6,
    COPY_NODE_CSV_RECORD = 7,
    COPY_REL_CSV_RECORD = 8,
    DROP_TABLE_RECORD = 9,
    DROP_PROPERTY_RECORD = 10,
};

string walRecordTypeToString(WALRecordType walRecordType);

struct PageUpdateOrInsertRecord {
    StorageStructureID storageStructureID;
    // PageIdx in the file of updated storage structure, identified by the storageStructureID field
    uint64_t pageIdxInOriginalFile;
    uint64_t pageIdxInWAL;
    bool isInsert;

    PageUpdateOrInsertRecord() = default;

    PageUpdateOrInsertRecord(StorageStructureID storageStructureID, uint64_t pageIdxInOriginalFile,
        uint64_t pageIdxInWAL, bool isInsert)
        : storageStructureID{storageStructureID}, pageIdxInOriginalFile{pageIdxInOriginalFile},
          pageIdxInWAL{pageIdxInWAL}, isInsert{isInsert} {}

    inline bool operator==(const PageUpdateOrInsertRecord& rhs) const {
        return storageStructureID == rhs.storageStructureID &&
               pageIdxInOriginalFile == rhs.pageIdxInOriginalFile &&
               pageIdxInWAL == rhs.pageIdxInWAL && isInsert == rhs.isInsert;
    }
};

struct CommitRecord {
    uint64_t transactionID;

    CommitRecord() = default;

    explicit CommitRecord(uint64_t transactionID) : transactionID{transactionID} {}

    inline bool operator==(const CommitRecord& rhs) const {
        return transactionID == rhs.transactionID;
    }
};

struct NodeTableRecord {
    table_id_t tableID;

    NodeTableRecord() = default;

    explicit NodeTableRecord(table_id_t tableID) : tableID{tableID} {}

    inline bool operator==(const NodeTableRecord& rhs) const { return tableID == rhs.tableID; }
};

struct RelTableRecord {
    table_id_t tableID;

    RelTableRecord() = default;

    explicit RelTableRecord(table_id_t tableID) : tableID{tableID} {}

    inline bool operator==(const RelTableRecord& rhs) const { return tableID == rhs.tableID; }
};

struct DiskOverflowFileNextBytePosRecord {
    StorageStructureID storageStructureID;
    uint64_t prevNextBytePosToWriteTo;

    DiskOverflowFileNextBytePosRecord() = default;

    DiskOverflowFileNextBytePosRecord(
        StorageStructureID storageStructureID, uint64_t prevNextByteToWriteTo)
        : storageStructureID{storageStructureID}, prevNextBytePosToWriteTo{prevNextByteToWriteTo} {}

    inline bool operator==(const DiskOverflowFileNextBytePosRecord& rhs) const {
        return storageStructureID == rhs.storageStructureID &&
               prevNextBytePosToWriteTo == rhs.prevNextBytePosToWriteTo;
    }
};

struct CopyNodeCSVRecord {
    table_id_t tableID;

    CopyNodeCSVRecord() = default;

    explicit CopyNodeCSVRecord(table_id_t tableID) : tableID{tableID} {}

    inline bool operator==(const CopyNodeCSVRecord& rhs) const { return tableID == rhs.tableID; }
};

struct CopyRelCSVRecord {
    table_id_t tableID;

    CopyRelCSVRecord() = default;

    explicit CopyRelCSVRecord(table_id_t tableID) : tableID{tableID} {}

    inline bool operator==(const CopyRelCSVRecord& rhs) const { return tableID == rhs.tableID; }
};

struct TableStatisticsRecord {
    bool isNodeTable;

    TableStatisticsRecord() = default;

    explicit TableStatisticsRecord(bool isNodeTable) : isNodeTable{isNodeTable} {}

    inline bool operator==(const TableStatisticsRecord& rhs) const {
        return isNodeTable == rhs.isNodeTable;
    }
};

struct DropTableRecord {
    table_id_t tableID;

    DropTableRecord() = default;

    DropTableRecord(table_id_t tableID) : tableID{tableID} {}

    inline bool operator==(const DropTableRecord& rhs) const { return tableID == rhs.tableID; }
};

struct DropPropertyRecord {
    table_id_t tableID;
    property_id_t propertyID;

    DropPropertyRecord() = default;

    DropPropertyRecord(table_id_t tableID, property_id_t propertyID)
        : tableID{tableID}, propertyID{propertyID} {}

    inline bool operator==(const DropPropertyRecord& rhs) const {
        return tableID == rhs.tableID && propertyID == rhs.propertyID;
    }
};

struct WALRecord {
    WALRecordType recordType;
    union {
        PageUpdateOrInsertRecord pageInsertOrUpdateRecord;
        CommitRecord commitRecord;
        NodeTableRecord nodeTableRecord;
        RelTableRecord relTableRecord;
        DiskOverflowFileNextBytePosRecord diskOverflowFileNextBytePosRecord;
        CopyNodeCSVRecord copyNodeCsvRecord;
        CopyRelCSVRecord copyRelCsvRecord;
        TableStatisticsRecord tableStatisticsRecord;
        DropTableRecord dropTableRecord;
        DropPropertyRecord dropPropertyRecord;
    };

    bool operator==(const WALRecord& rhs) const {
        if (recordType != rhs.recordType) {
            return false;
        }
        switch (recordType) {
        case WALRecordType::PAGE_UPDATE_OR_INSERT_RECORD: {
            return pageInsertOrUpdateRecord == rhs.pageInsertOrUpdateRecord;
        }
        case WALRecordType::COMMIT_RECORD: {
            return commitRecord == rhs.commitRecord;
        }
        case WALRecordType::TABLE_STATISTICS_RECORD: {
            return tableStatisticsRecord == rhs.tableStatisticsRecord;
        }
        case WALRecordType::CATALOG_RECORD: {
            // CatalogRecords are empty so are always equal
            return true;
        }
        case WALRecordType::NODE_TABLE_RECORD: {
            return nodeTableRecord == rhs.nodeTableRecord;
        }
        case WALRecordType::REL_TABLE_RECORD: {
            return relTableRecord == rhs.relTableRecord;
        }
        case WALRecordType::OVERFLOW_FILE_NEXT_BYTE_POS_RECORD: {
            return diskOverflowFileNextBytePosRecord == rhs.diskOverflowFileNextBytePosRecord;
        }
        case WALRecordType::COPY_NODE_CSV_RECORD: {
            return copyNodeCsvRecord == rhs.copyNodeCsvRecord;
        }
        case WALRecordType::COPY_REL_CSV_RECORD: {
            return copyRelCsvRecord == rhs.copyRelCsvRecord;
        }
        case WALRecordType::DROP_TABLE_RECORD: {
            return dropTableRecord == rhs.dropTableRecord;
        }
        case WALRecordType::DROP_PROPERTY_RECORD: {
            return dropPropertyRecord == rhs.dropPropertyRecord;
        }
        default: {
            throw RuntimeException("Unrecognized WAL record type inside ==. recordType: " +
                                   walRecordTypeToString(recordType));
        }
        }
    }

    static WALRecord newPageUpdateRecord(StorageStructureID storageStructureID_,
        uint64_t pageIdxInOriginalFile, uint64_t pageIdxInWAL);
    static WALRecord newPageInsertRecord(StorageStructureID storageStructureID_,
        uint64_t pageIdxInOriginalFile, uint64_t pageIdxInWAL);
    static WALRecord newCommitRecord(uint64_t transactionID);
    static WALRecord newTableStatisticsRecord(bool isNodeTable);
    static WALRecord newCatalogRecord();
    static WALRecord newNodeTableRecord(table_id_t tableID);
    static WALRecord newRelTableRecord(table_id_t tableID);
    static WALRecord newOverflowFileNextBytePosRecord(
        StorageStructureID storageStructureID_, uint64_t prevNextByteToWriteTo_);
    static WALRecord newCopyNodeCSVRecord(table_id_t tableID);
    static WALRecord newCopyRelCSVRecord(table_id_t tableID);
    static WALRecord newDropTableRecord(table_id_t tableID);
    static WALRecord newDropPropertyRecord(table_id_t tableID, property_id_t propertyID);
    static void constructWALRecordFromBytes(WALRecord& retVal, uint8_t* bytes, uint64_t& offset);
    // This functions assumes that the caller ensures there is enough space in the bytes pointer
    // to write the record. This should be checked by calling numBytesToWrite.
    void writeWALRecordToBytes(uint8_t* bytes, uint64_t& offset);

private:
    static WALRecord newPageInsertOrUpdateRecord(StorageStructureID storageStructureID_,
        uint64_t pageIdxInOriginalFile, uint64_t pageIdxInWAL, bool isInsert);
};

} // namespace storage
} // namespace kuzu
