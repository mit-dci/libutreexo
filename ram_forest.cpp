#include "uint256.h"
#include <iostream>
#include <ram_forest.h>

// RamForest::Node
const uint256& RamForest::Node::hash() const
{
    return this->h;
}

void RamForest::Node::reHash()
{
    // get the children hashes
    uint64_t leftChildPos = this->forestState.child(this->position, 0),
             rightChildPos = this->forestState.child(this->position, 1);
    uint256 leftChildHash = this->forest->read(leftChildPos),
            rightChildHash = this->forest->read(rightChildPos);

    // compute the hash
    this->h = Accumulator::parentHash(leftChildHash, rightChildHash);

    // write hash back
    uint8_t row = this->forestState.detectRow(this->position);
    uint64_t offset = this->forestState.rowOffset(this->position);
    std::vector<uint256>& rowData = this->forest->data.at(row);
    rowData[this->position - offset] = this->h;
}

std::shared_ptr<Accumulator::Node> RamForest::Node::parent() const
{
    uint64_t parentPos = this->forestState.parent(this->position);
    uint8_t row = this->forestState.detectRow(this->position);
    bool rowHasRoot = this->forestState.hasRoot(row);
    bool isRoot = this->forestState.rootPosition(row) == this->position;
    if (rowHasRoot && isRoot) {
        return nullptr;
    }

    return std::shared_ptr<Accumulator::Node>(
        (Accumulator::Node*)new RamForest::Node(this->forest, parentPos, uint256S("")));
}

// RamForest

const uint256 RamForest::read(uint64_t pos) const
{
    uint8_t row = this->state.detectRow(pos);
    uint64_t offset = this->state.rowOffset(pos);

    if (row >= this->data.size()) {
        // not enough rows
        std::cout << "not enough rows " << pos << std::endl;
        return uint256S("");
    }

    std::vector<uint256> rowData = this->data.at(row);

    if ((pos - offset) >= rowData.size()) {
        // row not big enough
        std::cout << "row not big enough " << pos << " " << +row << " " << rowData.size() << std::endl;
        return uint256S("");
    }

    return rowData.at(pos - offset);
}

void RamForest::swapRange(uint64_t from, uint64_t to, uint64_t range)
{
    uint8_t row = this->state.detectRow(from);
    uint64_t offsetFrom = this->state.rowOffset(from);
    uint64_t offsetTo = this->state.rowOffset(to);
    std::vector<uint256>& rowData = this->data.at(row);

    for (uint64_t i = 0; i < range; i++) {
        std::swap(rowData[(from - offsetFrom) + i], rowData[(to - offsetTo) + i]);
    }
}

std::vector<std::shared_ptr<Accumulator::Node>> RamForest::roots() const
{
    std::vector<uint64_t> vRoots = this->state.rootPositions();
    auto rootPosIt = vRoots.begin();
    std::vector<std::shared_ptr<Accumulator::Node>> forestRoots;
    while (rootPosIt < vRoots.end()) {
        // TODO: check for nullptr returned from read.
        // TODO: kinda ugly to cast away the const on "this" here.
        forestRoots.push_back(std::shared_ptr<Accumulator::Node>(
            (Accumulator::Node*)(new RamForest::Node((RamForest*)this, *rootPosIt, this->read(*rootPosIt)))));
        rootPosIt++;
    }

    return forestRoots;
}

std::shared_ptr<Accumulator::Node> RamForest::swapSubTrees(uint64_t posA, uint64_t posB)
{
    // posA and posB are on the same row
    uint8_t row = this->state.detectRow(posA);
    posA = this->state.leftDescendant(posA, row);
    posB = this->state.leftDescendant(posB, row);

    for (uint64_t range = 1 << row; range != 0; range >>= 1) {
        this->swapRange(posA, posB, range);
        posA = this->state.parent(posA);
        posB = this->state.parent(posB);
    }

    return std::shared_ptr<Accumulator::Node>((Accumulator::Node*)new RamForest::Node(this, posB, uint256S("")));
}

std::shared_ptr<Accumulator::Node> RamForest::mergeRoot(uint64_t parentPos, uint256 parentHash)
{
    // compute row
    uint8_t row = this->state.detectRow(parentPos);

    // add hash to forest
    this->data.at(row).push_back(parentHash);

    return std::shared_ptr<Accumulator::Node>((Accumulator::Node*)new RamForest::Node(
        this,
        parentPos,
        this->data.at(row).back()));
}

std::shared_ptr<Accumulator::Node> RamForest::newLeaf(uint256 hash)
{
    ForestState nextState(this->state.numLeaves + 1);
    if (nextState.numRows() > this->state.numRows()) {
        // append new row
        this->data.push_back(std::vector<uint256>());
    }

    // append new hash on row 0 (as a leaf)
    this->data.at(0).push_back(hash);
    return std::shared_ptr<Accumulator::Node>((Accumulator::Node*)new RamForest::Node(
        this,
        this->state.numLeaves,
        this->data.at(0).back()));
}

void RamForest::finalizeRemove(const ForestState nextState)
{
    uint64_t numLeaves = nextState.numLeaves;
    // Go through each row and resize the row vectors for the next forest state.
    for (uint8_t row = 0; row < this->state.numRows(); row++) {
        this->data.at(row).resize(numLeaves);
        // Compute the number of nodes in the next row.
        numLeaves >>= 1;
    }
}

const Accumulator::BatchProof RamForest::prove(const std::vector<uint64_t>& targets) const
{
    // TODO: check targets for validity like in remove.

    auto proofPositions = this->state.proofPositions(targets);
    std::vector<uint256> proof;
    for (uint64_t pos : proofPositions.first) {
        proof.push_back(this->read(pos));
    }

    return Accumulator::BatchProof(targets, proof);
}
