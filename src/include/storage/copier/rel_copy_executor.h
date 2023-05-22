#pragma once

#include "storage/copier/table_copy_executor.h"
#include "storage/in_mem_storage_structure/in_mem_column.h"
#include "storage/in_mem_storage_structure/in_mem_lists.h"
#include "storage/index/hash_index.h"
#include "storage/store/nodes_store.h"
#include "storage/store/rels_statistics.h"

namespace kuzu {
namespace storage {

class RelCopyExecutor : public TableCopyExecutor {
    enum class PopulateTaskType : uint8_t {
        populateAdjColumnsAndCountRelsInAdjListsTask = 0,
        populateListsTask = 1
    };

public:
    RelCopyExecutor(transaction::Transaction* transaction, common::CopyDescription& copyDescription,
        std::string outputDirectory, common::TaskScheduler& taskScheduler,
        catalog::Catalog& catalog, storage::NodesStore& nodesStore, storage::RelTable* table,
        RelsStatistics* relsStatistics);

private:
    void initializeColumnsAndLists() override;

    void populateColumnsAndLists(processor::ExecutionContext* executionContext) override;

    void saveToFile() override;

    void initializeColumns(common::RelDataDirection relDirection);

    void initializeLists(common::RelDataDirection relDirection);

    void initAdjListsHeaders();

    void initListsMetadata();

    inline void initializePkIndexes(common::table_id_t nodeTableID) {
        pkIndexes.emplace(nodeTableID, nodesStore.getPKIndex(nodeTableID));
    }

    void executePopulateTask(PopulateTaskType populateTaskType);

    void populateFromCSV(PopulateTaskType populateTaskType);

    void populateFromParquet(PopulateTaskType populateTaskType);

    void populateAdjColumnsAndCountRelsInAdjLists();

    void populateLists();

    // We store rel properties with overflows, e.g., strings or lists, in
    // InMemColumn/ListsWithOverflowFile (e.g., InMemStringLists). When loading these properties
    // from csv, we first save the overflow pointers of the ku_list_t or ku_string_t in temporary
    // "unordered" InMemOverflowFiles in an unordered format, instead of the InMemOverflowFiles of
    // the actual InMemColumn/ListsWithOverflowFile. In these temporary unordered
    // InMemOverflowFiles, the std::string of nodeOffset100's overflow data may be stored before
    // nodeOffset10's overflow data. This is because each thread gets a chunk of the csv file
    // storing rels but rel files are not necessarily sorted by source or destination node IDs.
    // Therefore, after populating an InMemColumn/ListWithOverflowFile we have to do two things: (1)
    // we need to copy over the data in these temporary unordered InMemOverflowFile to the
    // InMemOverflowFiles of the InMemColumn/ListsWithOverflowFile. (2) To increase the performance
    // of scanning these overflow files, we also sort the overflow pointers based on nodeOffsets, so
    // when scanning rels of consecutive nodes, the overflows of these rels appear consecutively on
    // disk.
    void sortAndCopyOverflowValues();

    template<typename T>
    static void inferTableIDsAndOffsets(const std::vector<std::shared_ptr<T>>& batchColumns,
        std::vector<common::nodeID_t>& nodeIDs, std::vector<common::LogicalType>& nodeIDTypes,
        const std::map<common::table_id_t, PrimaryKeyIndex*>& pkIndexes, int64_t blockOffset,
        int64_t& colIndex);

    template<typename T>
    static void putPropsOfLineIntoColumns(RelCopyExecutor* copier,
        std::vector<PageByteCursor>& inMemOverflowFileCursors,
        const std::vector<std::shared_ptr<T>>& batchColumns,
        const std::vector<common::nodeID_t>& nodeIDs, int64_t blockOffset, int64_t& colIndex);

    template<typename T>
    static void putPropsOfLineIntoLists(RelCopyExecutor* copier,
        std::vector<PageByteCursor>& inMemOverflowFileCursors,
        const std::vector<std::shared_ptr<T>>& batchColumns,
        const std::vector<common::nodeID_t>& nodeIDs, const std::vector<uint64_t>& reversePos,
        int64_t blockOffset, int64_t& colIndex, common::CopyDescription& copyDescription);

    static void copyStringOverflowFromUnorderedToOrderedPages(common::ku_string_t* kuStr,
        PageByteCursor& unorderedOverflowCursor, PageByteCursor& orderedOverflowCursor,
        InMemOverflowFile* unorderedOverflowFile, InMemOverflowFile* orderedOverflowFile);

    static void copyListOverflowFromUnorderedToOrderedPages(common::ku_list_t* kuList,
        const common::LogicalType& dataType, PageByteCursor& unorderedOverflowCursor,
        PageByteCursor& orderedOverflowCursor, InMemOverflowFile* unorderedOverflowFile,
        InMemOverflowFile* orderedOverflowFile);

    // Concurrent tasks.
    template<typename T>
    static void populateAdjColumnsAndCountRelsInAdjListsTask(uint64_t blockIdx,
        uint64_t blockStartRelID, RelCopyExecutor* copier,
        const std::vector<std::shared_ptr<T>>& batchColumns, const std::string& filePath);

    template<typename T>
    static void populateListsTask(uint64_t blockId, uint64_t blockStartRelID,
        RelCopyExecutor* copier, const std::vector<std::shared_ptr<T>>& batchColumns,
        const std::string& filePath);

    static void sortOverflowValuesOfPropertyColumnTask(const common::LogicalType& dataType,
        common::offset_t offsetStart, common::offset_t offsetEnd, InMemColumnChunk* propertyColumn,
        InMemOverflowFile* unorderedInMemOverflowFile, InMemOverflowFile* orderedInMemOverflowFile);

    static void sortOverflowValuesOfPropertyListsTask(const common::LogicalType& dataType,
        common::offset_t offsetStart, common::offset_t offsetEnd, InMemAdjLists* adjLists,
        InMemLists* propertyLists, InMemOverflowFile* unorderedInMemOverflowFile,
        InMemOverflowFile* orderedInMemOverflowFile);

    static void putValueIntoColumns(uint64_t propertyIdx,
        std::vector<std::unordered_map<common::property_id_t, std::unique_ptr<InMemColumnChunk>>>&
            directionTablePropertyColumns,
        const std::vector<common::nodeID_t>& nodeIDs, uint8_t* val);

    static void putValueIntoLists(uint64_t propertyIdx,
        std::vector<std::unordered_map<common::property_id_t, std::unique_ptr<InMemLists>>>&
            directionTablePropertyLists,
        std::vector<std::unique_ptr<InMemAdjLists>>& directionTableAdjLists,
        const std::vector<common::nodeID_t>& nodeIDs, const std::vector<uint64_t>& reversePos,
        uint8_t* val);

    // Initializes (in listHeadersBuilder) the header of each list in a Lists structure, from the
    // listSizes. ListSizes is used to determine if the list is small or large, based on which,
    // information is encoded in the 4 byte header.
    static void calculateListHeadersTask(common::offset_t numNodes, atomic_uint64_vec_t* listSizes,
        ListHeadersBuilder* listHeadersBuilder, const std::shared_ptr<spdlog::logger>& logger);

    // Initializes Metadata information of a Lists structure, that is chunksPagesMap and
    // largeListsPagesMap, using listSizes and listHeadersBuilder.
    // **Note that this file also allocates the in-memory pages of the InMemFile that will actually
    // store the data in the lists (e.g., neighbor ids or edge properties).
    static void calculateListsMetadataAndAllocateInMemListPagesTask(uint64_t numNodes,
        uint32_t elementSize, atomic_uint64_vec_t* listSizes,
        ListHeadersBuilder* listHeadersBuilder, InMemLists* inMemList, bool hasNULLBytes,
        const std::shared_ptr<spdlog::logger>& logger);

private:
    storage::NodesStore& nodesStore;
    storage::RelTable* table;
    const std::map<common::table_id_t, common::offset_t> maxNodeOffsetsPerTable;
    std::map<common::table_id_t, PrimaryKeyIndex*> pkIndexes;
    std::atomic<uint64_t> numRels = 0;
    std::vector<std::unique_ptr<atomic_uint64_vec_t>> listSizesPerDirection{2};
    std::vector<std::unique_ptr<InMemColumnChunk>> adjColumnChunksPerDirection{2};
    std::vector<std::unique_ptr<InMemColumn>> adjColumnsPerDirection{2};
    std::vector<std::unordered_map<common::property_id_t, std::unique_ptr<InMemColumnChunk>>>
        propertyColumnChunksPerDirection{2};
    std::vector<std::unordered_map<common::property_id_t, std::unique_ptr<InMemColumn>>>
        propertyColumnsPerDirection{2};
    std::vector<std::unique_ptr<InMemAdjLists>> adjListsPerDirection{2};
    std::vector<std::unordered_map<common::property_id_t, std::unique_ptr<InMemLists>>>
        propertyListsPerDirection{2};
    std::unordered_map<common::property_id_t, std::unique_ptr<InMemOverflowFile>>
        overflowFilePerPropertyID;
};

} // namespace storage
} // namespace kuzu
