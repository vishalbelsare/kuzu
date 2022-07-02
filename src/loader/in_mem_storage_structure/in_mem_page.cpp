#include "src/loader/in_mem_storage_structure/include/in_mem_page.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace graphflow {
namespace loader {

InMemPage::InMemPage(uint32_t maxNumElements, uint16_t numBytesForElement, bool hasNullEntries)
    : nullEntriesInPage{nullptr}, maxNumElements{maxNumElements} {
    buffer = make_unique<uint8_t[]>(DEFAULT_PAGE_SIZE);
    data = buffer.get();
    if (hasNullEntries) {
        // In a page, null entries are stored right after the element data. Each null entry contains
        // 64 bits, and one bit indicates an element with corresponding position in this page is
        // null or not. By default, we consider all elements in the page to be NULL. When a new
        // element comes in to be put at a pos, its respective NULL bit is reset to denote a
        // non-NULL value.
        nullEntriesInPage = (uint64_t*)(data + (numBytesForElement * maxNumElements));
        auto numNullEntries = (maxNumElements + NullMask::NUM_BITS_PER_NULL_ENTRY - 1) /
                              NullMask::NUM_BITS_PER_NULL_ENTRY;
        fill(nullEntriesInPage, nullEntriesInPage + numNullEntries, NullMask::ALL_NULL_ENTRY);
        nullMask = make_unique<uint8_t[]>(maxNumElements);
        memset(nullMask.get(), UINT8_MAX, maxNumElements);
    }
}

void InMemPage::setElementAtPosToNonNull(uint32_t pos) {
    auto entryPos = pos >> NullMask::NUM_BITS_PER_NULL_ENTRY_LOG2;
    auto bitPosInEntry = pos - (entryPos << NullMask::NUM_BITS_PER_NULL_ENTRY_LOG2);
    nullEntriesInPage[entryPos] &= NULL_BITMASKS_WITH_SINGLE_ZERO[bitPosInEntry];
}

uint8_t* InMemPage::write(nodeID_t* nodeID, uint32_t byteOffsetInPage, uint32_t elemPosInPage,
    const NodeIDCompressionScheme& compressionScheme) {
    memcpy(data + byteOffsetInPage, &nodeID->label, compressionScheme.getNumBytesForLabel());
    memcpy(data + byteOffsetInPage + compressionScheme.getNumBytesForLabel(), &nodeID->offset,
        compressionScheme.getNumBytesForOffset());
    if (nullMask) {
        nullMask[elemPosInPage] = false;
    }
    return data + byteOffsetInPage;
}

uint8_t* InMemPage::write(uint32_t byteOffsetInPage, uint32_t elemPosInPage, const uint8_t* elem,
    uint32_t numBytesForElem) {
    memcpy(data + byteOffsetInPage, elem, numBytesForElem);
    if (nullMask) {
        nullMask[elemPosInPage] = false;
    }
    return data + byteOffsetInPage;
}

void InMemPage::encodeNullBits() {
    if (nullMask == nullptr) {
        return;
    }
    for (auto i = 0u; i < maxNumElements; i++) {
        if (!nullMask[i]) {
            setElementAtPosToNonNull(i);
        }
    }
}

} // namespace loader
} // namespace graphflow
