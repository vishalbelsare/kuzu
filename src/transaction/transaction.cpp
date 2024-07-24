#include "transaction/transaction.h"

#include "catalog/catalog_entry/table_catalog_entry.h"
#include "common/exception/runtime.h"
#include "main/client_context.h"
#include "storage/local_storage/local_storage.h"
#include "storage/store/version_info.h"
#include "storage/undo_buffer.h"
#include "storage/wal/wal.h"

using namespace kuzu::catalog;

namespace kuzu {
namespace transaction {

Transaction::Transaction(main::ClientContext& clientContext, TransactionType transactionType,
    common::transaction_t transactionID, common::transaction_t startTS)
    : type{transactionType}, ID{transactionID}, startTS{startTS},
      commitTS{common::INVALID_TRANSACTION}, forceCheckpoint{false} {
    this->clientContext = &clientContext;
    localStorage = std::make_unique<storage::LocalStorage>(clientContext);
    undoBuffer = std::make_unique<storage::UndoBuffer>(this);
    currentTS = common::Timestamp::getCurrentTimestamp().value;
}

void Transaction::commit(storage::WAL* wal) const {
    localStorage->commit();
    undoBuffer->commit(commitTS);
    if (isWriteTransaction()) {
        KU_ASSERT(wal);
        wal->logAndFlushCommit(ID);
    }
}

void Transaction::rollback() const {
    localStorage->rollback();
    undoBuffer->rollback();
}

void Transaction::pushCatalogEntry(CatalogSet& catalogSet, CatalogEntry& catalogEntry) const {
    undoBuffer->createCatalogEntry(catalogSet, catalogEntry);
    if (isRecovery()) {
        return;
    }
    auto wal = clientContext->getWAL();
    KU_ASSERT(wal);
    const auto newCatalogEntry = catalogEntry.getNext();
    switch (newCatalogEntry->getType()) {
    case CatalogEntryType::NODE_TABLE_ENTRY:
    case CatalogEntryType::REL_TABLE_ENTRY:
    case CatalogEntryType::REL_GROUP_ENTRY:
    case CatalogEntryType::RDF_GRAPH_ENTRY: {
        if (catalogEntry.getType() == CatalogEntryType::DUMMY_ENTRY) {
            KU_ASSERT(catalogEntry.isDeleted());
            wal->logCreateCatalogEntryRecord(newCatalogEntry);
        } else {
            // Must be alter.
            KU_ASSERT(catalogEntry.getType() == newCatalogEntry->getType());
            const auto& tableEntry = catalogEntry.constCast<TableCatalogEntry>();
            wal->logAlterTableEntryRecord(tableEntry.getAlterInfo());
        }
    } break;
    case CatalogEntryType::SCALAR_MACRO_ENTRY:
    case CatalogEntryType::SEQUENCE_ENTRY:
    case CatalogEntryType::TYPE_ENTRY: {
        KU_ASSERT(
            catalogEntry.getType() == CatalogEntryType::DUMMY_ENTRY && catalogEntry.isDeleted());
        wal->logCreateCatalogEntryRecord(newCatalogEntry);
    } break;
    case CatalogEntryType::DUMMY_ENTRY: {
        KU_ASSERT(newCatalogEntry->isDeleted());
        switch (catalogEntry.getType()) {
        // Eventually we probably want to merge these
        case CatalogEntryType::NODE_TABLE_ENTRY:
        case CatalogEntryType::REL_TABLE_ENTRY:
        case CatalogEntryType::REL_GROUP_ENTRY:
        case CatalogEntryType::RDF_GRAPH_ENTRY: {
            const auto tableCatalogEntry = catalogEntry.constPtrCast<TableCatalogEntry>();
            wal->logDropCatalogEntryRecord(tableCatalogEntry->getTableID(), catalogEntry.getType());
        } break;
        case CatalogEntryType::SEQUENCE_ENTRY: {
            auto sequenceCatalogEntry = catalogEntry.constPtrCast<SequenceCatalogEntry>();
            wal->logDropCatalogEntryRecord(sequenceCatalogEntry->getSequenceID(),
                catalogEntry.getType());
        } break;
        case CatalogEntryType::SCALAR_FUNCTION_ENTRY: {
            // DO NOTHING. We don't persistent function entries.
        } break;
        case CatalogEntryType::SCALAR_MACRO_ENTRY:
        case CatalogEntryType::TYPE_ENTRY:
        default: {
            throw common::RuntimeException(
                common::stringFormat("Not supported catalog entry type {} yet.",
                    CatalogEntryTypeUtils::toString(catalogEntry.getType())));
        }
        }
    } break;
    case CatalogEntryType::SCALAR_FUNCTION_ENTRY: {
        // DO NOTHING. We don't persistent function entries.
    } break;
    default: {
        throw common::RuntimeException(
            common::stringFormat("Not supported catalog entry type {} yet.",
                CatalogEntryTypeUtils::toString(catalogEntry.getType())));
    }
    }
}

void Transaction::pushSequenceChange(SequenceCatalogEntry* sequenceEntry, int64_t kCount,
    const SequenceRollbackData& data) const {
    undoBuffer->createSequenceChange(*sequenceEntry, data);
    clientContext->getWAL()->logUpdateSequenceRecord(sequenceEntry->getSequenceID(), kCount);
}

void Transaction::pushVectorInsertInfo(storage::VersionInfo& versionInfo,
    const common::idx_t vectorIdx, common::row_idx_t startRowInVector,
    common::row_idx_t numRows) const {
    undoBuffer->createVectorInsertInfo(&versionInfo, vectorIdx, startRowInVector, numRows);
}

void Transaction::pushVectorDeleteInfo(storage::VersionInfo& versionInfo,
    const common::idx_t vectorIdx, common::row_idx_t startRowInVector,
    common::row_idx_t numRows) const {
    undoBuffer->createVectorDeleteInfo(&versionInfo, vectorIdx, startRowInVector, numRows);
}

void Transaction::pushVectorUpdateInfo(storage::UpdateInfo& updateInfo,
    const common::idx_t vectorIdx, storage::VectorUpdateInfo& vectorUpdateInfo) const {
    undoBuffer->createVectorUpdateInfo(&updateInfo, vectorIdx, &vectorUpdateInfo);
}

} // namespace transaction
} // namespace kuzu
