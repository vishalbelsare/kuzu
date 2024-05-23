#include "graph/on_disk_graph.h"

#include "storage/storage_manager.h"

using namespace kuzu::catalog;
using namespace kuzu::storage;
using namespace kuzu::main;
using namespace kuzu::common;

namespace kuzu {
namespace graph {

NbrScanState::NbrScanState(storage::MemoryManager* mm) {
    srcNodeIDVectorState = DataChunkState::getSingleValueDataChunkState();
    dstNodeIDVectorState = std::make_shared<common::DataChunkState>();
    srcNodeIDVector = std::make_unique<ValueVector>(*LogicalType::INTERNAL_ID(), mm);
    srcNodeIDVector->state = srcNodeIDVectorState;
    dstNodeIDVector = std::make_unique<ValueVector>(*LogicalType::INTERNAL_ID(), mm);
    dstNodeIDVector->state = dstNodeIDVectorState;
    fwdReadState = std::make_unique<RelTableReadState>(columnIDs, direction);
    fwdReadState->nodeIDVector = srcNodeIDVector.get();
    fwdReadState->outputVectors.push_back(dstNodeIDVector.get());
}

OnDiskGraph::OnDiskGraph(main::ClientContext* context, const std::string& nodeName,
    const std::string& relName)
    : context{context} {
    auto catalog = context->getCatalog();
    auto storage = context->getStorageManager();
    auto tx = context->getTx();
    auto nodeTableID = catalog->getTableID(tx, nodeName);
    nodeTable = storage->getTable(nodeTableID)->ptrCast<NodeTable>();
    auto relTableID = catalog->getTableID(tx, relName);
    relTable = storage->getTable(relTableID)->ptrCast<RelTable>();
    nbrScanState = std::make_unique<NbrScanState>(context->getMemoryManager());
}

common::offset_t OnDiskGraph::getNumNodes() {
    return nodeTable->getNumTuples(context->getTx());
}

common::offset_t OnDiskGraph::getNumEdges() {
    return relTable->getNumTuples(context->getTx());
}

std::vector<common::nodeID_t> OnDiskGraph::getNbrs(common::offset_t offset) {
    nbrScanState->srcNodeIDVector->setValue<nodeID_t>(0, {offset, nodeTable->getTableID()});
    auto tx = context->getTx();
    auto readState = nbrScanState->fwdReadState.get();
    auto dstState = nbrScanState->dstNodeIDVectorState.get();
    auto dstVector = nbrScanState->dstNodeIDVector.get();
    std::vector<nodeID_t> nbrs;
    relTable->initializeReadState(tx, RelDataDirection::FWD, nbrScanState->columnIDs, *readState);
    while (nbrScanState->fwdReadState->hasMoreToRead(tx)) {
        relTable->read(tx, *readState);
        KU_ASSERT(dstState->getSelVector().isUnfiltered());
        for (auto i = 0u; i < dstState->getSelVector().getSelSize(); ++i) {
            auto nodeID = dstVector->getValue<nodeID_t>(i);
            nbrs.push_back(nodeID);
        }
    }
    return nbrs;
}

} // namespace graph
} // namespace kuzu
