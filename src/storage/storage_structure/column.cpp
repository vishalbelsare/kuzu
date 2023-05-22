#include "storage/storage_structure/column.h"

#include "common/in_mem_overflow_buffer_utils.h"
#include "storage/storage_structure/storage_structure_utils.h"

using namespace kuzu::common;
using namespace kuzu::transaction;

namespace kuzu {
namespace storage {

Column::Column(const kuzu::storage::StorageStructureIDAndFName& structureIDAndFName,
    const common::LogicalType& dataType, size_t elementSize,
    kuzu::storage::BufferManager* bufferManager, kuzu::storage::WAL* wal, bool requireNullBits)
    : BaseColumnOrList{structureIDAndFName, dataType, elementSize, bufferManager,
          false /*hasNULLBytes*/, wal} {
    readDataFunc = Column::readValuesFromPage;
    writeDataFunc = Column::writeValueToPage;
    if (requireNullBits) {
        auto nullColumnStructureIDAndFName =
            StorageUtils::getNodeNullColumnStructureIDAndFName(structureIDAndFName);
        nullColumn =
            std::make_unique<NullColumn>(nullColumnStructureIDAndFName, bufferManager, wal);
    }
}

void Column::batchLookup(const common::offset_t* nodeOffsets, size_t size, uint8_t* result) {
    for (auto i = 0u; i < size; ++i) {
        auto nodeOffset = nodeOffsets[i];
        auto cursor = PageUtils::getPageElementCursorForPos(nodeOffset, numElementsPerPage);
        readFromPage(const_cast<Transaction*>(&DUMMY_READ_ONLY_TRX), cursor.pageIdx,
            [&](uint8_t* frame) -> void {
                auto frameBytesOffset = getElemByteOffset(cursor.elemPosInPage);
                memcpy(result + i * elementSize, frame + frameBytesOffset, elementSize);
            });
    }
}

void Column::read(Transaction* txn, common::ValueVector* nodeIDVector,
    common::ValueVector* resultVector) {
    if (nullColumn) {
        nullColumn->read(txn, nodeIDVector, resultVector);
    }
    if (nodeIDVector->state->isFlat()) {
        auto pos = nodeIDVector->state->selVector->selectedPositions[0];
        lookup(txn, nodeIDVector, resultVector, pos);
    } else if (nodeIDVector->isSequential()) {
        scan(txn, nodeIDVector, resultVector);
    } else {
        for (auto i = 0ul; i < nodeIDVector->state->selVector->selectedSize; i++) {
            auto pos = nodeIDVector->state->selVector->selectedPositions[i];
            lookup(txn, nodeIDVector, resultVector, pos);
        }
    }
}

void Column::write(Transaction* txn, common::ValueVector* nodeIDVector,
    common::ValueVector* vectorToWriteFrom) {
    if (nodeIDVector->state->isFlat() && vectorToWriteFrom->state->isFlat()) {
        auto nodeOffset =
            nodeIDVector->readNodeOffset(nodeIDVector->state->selVector->selectedPositions[0]);
        write(txn, nodeOffset, vectorToWriteFrom,
            vectorToWriteFrom->state->selVector->selectedPositions[0]);
    } else if (nodeIDVector->state->isFlat() && !vectorToWriteFrom->state->isFlat()) {
        auto nodeOffset =
            nodeIDVector->readNodeOffset(nodeIDVector->state->selVector->selectedPositions[0]);
        auto lastPos = vectorToWriteFrom->state->selVector->selectedSize - 1;
        write(txn, nodeOffset, vectorToWriteFrom, lastPos);
    } else if (!nodeIDVector->state->isFlat() && vectorToWriteFrom->state->isFlat()) {
        for (auto i = 0u; i < nodeIDVector->state->selVector->selectedSize; ++i) {
            auto nodeOffset =
                nodeIDVector->readNodeOffset(nodeIDVector->state->selVector->selectedPositions[i]);
            write(txn, nodeOffset, vectorToWriteFrom,
                vectorToWriteFrom->state->selVector->selectedPositions[0]);
        }
    } else if (!nodeIDVector->state->isFlat() && !vectorToWriteFrom->state->isFlat()) {
        for (auto i = 0u; i < nodeIDVector->state->selVector->selectedSize; ++i) {
            auto pos = nodeIDVector->state->selVector->selectedPositions[i];
            auto nodeOffset = nodeIDVector->readNodeOffset(pos);
            write(txn, nodeOffset, vectorToWriteFrom, pos);
        }
    }
}

bool Column::isNull(common::offset_t nodeOffset, transaction::Transaction* txn) {
    return nullColumn->readValue(nodeOffset, txn);
}

void Column::setNull(common::offset_t nodeOffset) {
    nullColumn->setValue(nodeOffset);
}

Value Column::readValueForTestingOnly(offset_t offset) {
    auto cursor = PageUtils::getPageElementCursorForPos(offset, numElementsPerPage);
    Value retVal = Value::createDefaultValue(dataType);
    readFromPage(const_cast<Transaction*>(&DUMMY_READ_ONLY_TRX), cursor.pageIdx,
        [&](uint8_t* frame) {
            retVal.copyValueFrom(frame + mapElementPosToByteOffset(cursor.elemPosInPage));
        });
    return retVal;
}

void Column::lookup(Transaction* txn, common::ValueVector* nodeIDVector,
    common::ValueVector* resultVector, uint32_t vectorPos) {
    if (nodeIDVector->isNull(vectorPos)) {
        resultVector->setNull(vectorPos, true);
        return;
    }
    auto nodeOffset = nodeIDVector->readNodeOffset(vectorPos);
    lookup(txn, nodeOffset, resultVector, vectorPos);
}

void Column::lookup(Transaction* txn, common::offset_t nodeOffset,
    common::ValueVector* resultVector, uint32_t vectorPos) {
    auto pageCursor = PageUtils::getPageElementCursorForPos(nodeOffset, numElementsPerPage);
    readFromPage(txn, pageCursor.pageIdx, [&](uint8_t* frame) {
        readDataFunc(txn, frame, pageCursor, resultVector, vectorPos, 1, diskOverflowFile.get());
    });
}

void Column::scan(transaction::Transaction* txn, common::ValueVector* nodeIDVector,
    common::ValueVector* resultVector) {
    // In sequential read, we fetch start offset regardless of selected position.
    auto startOffset = nodeIDVector->readNodeOffset(0);
    uint64_t numValuesToRead = nodeIDVector->state->originalSize;
    auto pageCursor = PageUtils::getPageElementCursorForPos(startOffset, numElementsPerPage);
    auto numValuesRead = 0u;
    auto posInSelVector = 0u;
    if (nodeIDVector->state->selVector->isUnfiltered()) {
        while (numValuesRead < numValuesToRead) {
            uint64_t numValuesToReadInPage =
                std::min((uint64_t)numElementsPerPage - pageCursor.elemPosInPage,
                    numValuesToRead - numValuesRead);
            readFromPage(txn, pageCursor.pageIdx, [&](uint8_t* frame) -> void {
                readDataFunc(txn, frame, pageCursor, resultVector, numValuesRead,
                    numValuesToReadInPage, diskOverflowFile.get());
            });
            numValuesRead += numValuesToReadInPage;
            pageCursor.nextPage();
        }
    } else {
        while (numValuesRead < numValuesToRead) {
            uint64_t numValuesToReadInPage =
                std::min((uint64_t)numElementsPerPage - pageCursor.elemPosInPage,
                    numValuesToRead - numValuesRead);
            if (isInRange(nodeIDVector->state->selVector->selectedPositions[posInSelVector],
                    numValuesRead, numValuesRead + numValuesToReadInPage)) {
                readFromPage(txn, pageCursor.pageIdx, [&](uint8_t* frame) -> void {
                    readDataFunc(txn, frame, pageCursor, resultVector, numValuesRead,
                        numValuesToReadInPage, diskOverflowFile.get());
                });
            }
            numValuesRead += numValuesToReadInPage;
            pageCursor.nextPage();
            while (
                posInSelVector < nodeIDVector->state->selVector->selectedSize &&
                nodeIDVector->state->selVector->selectedPositions[posInSelVector] < numValuesRead) {
                posInSelVector++;
            }
        }
    }
}

void Column::write(Transaction* txn, common::offset_t nodeOffset,
    common::ValueVector* vectorToWriteFrom, uint32_t posInVectorToWriteFrom) {
    bool isNull = vectorToWriteFrom->isNull(posInVectorToWriteFrom);
    if (nullColumn) {
        nullColumn->write(txn, nodeOffset, vectorToWriteFrom, posInVectorToWriteFrom);
    }
    if (!isNull) {
        auto walPageInfo =
            createWALVersionOfPageIfNecessaryForElement(nodeOffset, numElementsPerPage);
        try {
            writeDataFunc(walPageInfo.frame, walPageInfo.posInPage, vectorToWriteFrom,
                posInVectorToWriteFrom, diskOverflowFile.get());
        } catch (Exception& e) {
            bufferManager->unpin(*wal->fileHandle, walPageInfo.pageIdxInWAL);
            fileHandle->releaseWALPageIdxLock(walPageInfo.originalPageIdx);
            throw;
        }
        bufferManager->unpin(*wal->fileHandle, walPageInfo.pageIdxInWAL);
        fileHandle->releaseWALPageIdxLock(walPageInfo.originalPageIdx);
    }
}

void Column::readFromPage(transaction::Transaction* txn, common::page_idx_t pageIdx,
    const std::function<void(uint8_t*)>& func) {
    auto [fileHandleToPin, pageIdxToPin] =
        StorageStructureUtils::getFileHandleAndPhysicalPageIdxToPin(*fileHandle, pageIdx, *wal,
            txn->getType());
    bufferManager->optimisticRead(*fileHandleToPin, pageIdxToPin, func);
}

void Column::readValuesFromPage(transaction::Transaction* txn, uint8_t* frame,
    PageElementCursor& pageCursor, common::ValueVector* resultVector, uint32_t posInVector,
    uint32_t numValuesToRead, DiskOverflowFile* diskOverflowFile) {
    auto numBytesPerValue = resultVector->getNumBytesPerValue();
    memcpy(resultVector->getData() + posInVector * numBytesPerValue,
        frame + pageCursor.elemPosInPage * numBytesPerValue, numValuesToRead * numBytesPerValue);
}

void Column::writeValueToPage(uint8_t* frame, uint16_t posInFrame, common::ValueVector* vector,
    uint32_t posInVector, kuzu::storage::DiskOverflowFile* diskOverflowFile) {
    auto numBytesPerValue = vector->getNumBytesPerValue();
    memcpy(frame + posInFrame * numBytesPerValue,
        vector->getData() + posInVector * numBytesPerValue, numBytesPerValue);
}

void NullColumn::write(Transaction* txn, common::offset_t nodeOffset,
    common::ValueVector* vectorToWriteFrom, uint32_t posInVectorToWriteFrom) {
    auto walPageInfo = createWALVersionOfPageIfNecessaryForElement(nodeOffset, numElementsPerPage);
    *(walPageInfo.frame + walPageInfo.posInPage) =
        vectorToWriteFrom->isNull(posInVectorToWriteFrom);
    bufferManager->unpin(*wal->fileHandle, walPageInfo.pageIdxInWAL);
    fileHandle->releaseWALPageIdxLock(walPageInfo.originalPageIdx);
}

bool NullColumn::readValue(offset_t nodeOffset, Transaction* txn) {
    auto pageCursor = PageUtils::getPageElementCursorForPos(nodeOffset, numElementsPerPage);
    bool isNull;
    readFromPage(txn, pageCursor.pageIdx,
        [&](uint8_t* frame) -> void { isNull = *(frame + pageCursor.elemPosInPage); });
    return isNull;
}

void NullColumn::setValue(common::offset_t nodeOffset, bool isNull) {
    auto walPageInfo = createWALVersionOfPageIfNecessaryForElement(nodeOffset, numElementsPerPage);
    *(walPageInfo.frame + walPageInfo.posInPage) = isNull;
    StorageStructureUtils::unpinWALPageAndReleaseOriginalPageLock(walPageInfo, *fileHandle,
        *bufferManager, *wal);
}

void NullColumn::readNullsFromPage(transaction::Transaction* txn, uint8_t* frame,
    kuzu::storage::PageElementCursor& pageCursor, common::ValueVector* resultVector,
    uint32_t posInVector, uint32_t numValuesToRead,
    kuzu::storage::DiskOverflowFile* diskOverflowFile) {
    for (auto i = 0u; i < numValuesToRead; i++) {
        bool isNull = *(frame + pageCursor.elemPosInPage + i);
        resultVector->setNull(posInVector + i, isNull);
    }
}

Value StringPropertyColumn::readValueForTestingOnly(offset_t offset) {
    ku_string_t kuString;
    auto cursor = PageUtils::getPageElementCursorForPos(offset, numElementsPerPage);
    readFromPage(const_cast<Transaction*>(&DUMMY_READ_ONLY_TRX), cursor.pageIdx,
        [&](uint8_t* frame) -> void {
            memcpy(&kuString, frame + mapElementPosToByteOffset(cursor.elemPosInPage),
                sizeof(ku_string_t));
        });
    return Value(diskOverflowFile->readString(TransactionType::READ_ONLY, kuString));
}

void StringPropertyColumn::writeStringToPage(uint8_t* frame, uint16_t posInFrame,
    common::ValueVector* vector, uint32_t posInVector,
    kuzu::storage::DiskOverflowFile* diskOverflowFile) {
    auto stringToWriteTo = (ku_string_t*)(frame + (posInFrame * sizeof(ku_string_t)));
    auto stringToWriteFrom = vector->getValue<ku_string_t>(posInVector);
    memcpy(stringToWriteTo, &stringToWriteFrom, sizeof(ku_string_t));
    // If the string we write is a long string, it's overflowPtr is currently pointing to
    // the overflow buffer of vectorToWriteFrom. We need to move it to storage.
    if (!ku_string_t::isShortString(stringToWriteFrom.len)) {
        diskOverflowFile->writeStringOverflowAndUpdateOverflowPtr(stringToWriteFrom,
            *stringToWriteTo);
    }
}

Value ListPropertyColumn::readValueForTestingOnly(offset_t offset) {
    ku_list_t kuList;
    auto cursor = PageUtils::getPageElementCursorForPos(offset, numElementsPerPage);
    readFromPage(const_cast<Transaction*>(&DUMMY_READ_ONLY_TRX), cursor.pageIdx,
        [&](uint8_t* frame) -> void {
            memcpy(&kuList, frame + mapElementPosToByteOffset(cursor.elemPosInPage),
                sizeof(ku_list_t));
        });
    return Value(dataType,
        diskOverflowFile->readList(TransactionType::READ_ONLY, kuList, dataType));
}

void ListPropertyColumn::readListsFromPage(transaction::Transaction* txn, uint8_t* frame,
    kuzu::storage::PageElementCursor& pageCursor, common::ValueVector* resultVector,
    uint32_t posInVector, uint32_t numValuesToRead, DiskOverflowFile* diskOverflowFile) {
    auto frameBytesOffset = pageCursor.elemPosInPage * sizeof(ku_list_t);
    auto kuListsToRead = reinterpret_cast<common::ku_list_t*>(frame + frameBytesOffset);
    for (auto i = 0u; i < numValuesToRead; i++) {
        if (!resultVector->isNull(posInVector + i)) {
            diskOverflowFile->readListToVector(txn->getType(), kuListsToRead[i], resultVector,
                posInVector + i);
        }
    }
}

void ListPropertyColumn::writeListToPage(uint8_t* frame, uint16_t posInFrame,
    common::ValueVector* vector, uint32_t posInVector,
    kuzu::storage::DiskOverflowFile* diskOverflowFile) {
    auto kuListToWriteTo = (ku_list_t*)(frame + (posInFrame * sizeof(ku_list_t)));
    auto kuListToWriteFrom = vector->getValue<ku_list_t>(posInVector);
    memcpy(kuListToWriteTo, &kuListToWriteFrom, sizeof(ku_list_t));
    diskOverflowFile->writeListOverflowAndUpdateOverflowPtr(kuListToWriteFrom, *kuListToWriteTo,
        vector->dataType);
}

StructPropertyColumn::StructPropertyColumn(const StorageStructureIDAndFName& structureIDAndFName,
    const common::LogicalType& dataType, BufferManager* bufferManager, WAL* wal)
    : Column{dataType} {
    auto structFields = common::StructType::getStructFields(&dataType);
    for (auto structField : structFields) {
        auto fieldStructureIDAndFName = structureIDAndFName;
        fieldStructureIDAndFName.fName = StorageUtils::appendStructFieldName(
            fieldStructureIDAndFName.fName, structField->getName());
        structFieldColumns.push_back(ColumnFactory::getColumn(fieldStructureIDAndFName,
            *structField->getType(), bufferManager, wal));
    }
}

void StructPropertyColumn::read(Transaction* txn, common::ValueVector* nodeIDVector,
    common::ValueVector* resultVector) {
    // TODO(Guodong/Ziyi): We currently do not support null struct value.
    resultVector->setAllNonNull();
    for (auto i = 0u; i < structFieldColumns.size(); i++) {
        structFieldColumns[i]->read(txn, nodeIDVector,
            common::StructVector::getChildVector(resultVector, i).get());
    }
}

void InternalIDColumn::readInternalIDsFromPage(transaction::Transaction* txn, uint8_t* frame,
    PageElementCursor& pageCursor, common::ValueVector* resultVector, uint32_t posInVector,
    uint32_t numValuesToRead, DiskOverflowFile* diskOverflowFile) {
    auto resultData = (internalID_t*)resultVector->getData();
    for (auto i = 0u; i < numValuesToRead; i++) {
        auto posInFrame = pageCursor.elemPosInPage + i;
        resultData[posInVector + i].offset = *(offset_t*)(frame + (posInFrame * sizeof(offset_t)));
    }
}

void InternalIDColumn::writeInternalIDToPage(uint8_t* frame, uint16_t posInFrame,
    common::ValueVector* vector, uint32_t posInVector,
    kuzu::storage::DiskOverflowFile* diskOverflowFile) {
    auto relID = vector->getValue<common::relID_t>(posInVector);
    memcpy(frame + posInFrame * sizeof(offset_t), &relID.offset, sizeof(offset_t));
}

void SerialColumn::read(transaction::Transaction* txn, common::ValueVector* nodeIDVector,
    common::ValueVector* resultVector) {
    // Serial column cannot contain null values.
    for (auto i = 0ul; i < nodeIDVector->state->selVector->selectedSize; i++) {
        auto pos = nodeIDVector->state->selVector->selectedPositions[i];
        auto offset = nodeIDVector->readNodeOffset(pos);
        resultVector->setValue<offset_t>(pos, offset);
    }
}

} // namespace storage
} // namespace kuzu
