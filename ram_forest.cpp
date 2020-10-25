#include <iostream>
#include <ram_forest.h>

// RamForest::Node
const uint256& RamForest::Node::hash() const
{
    return this->h;
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

std::vector<std::shared_ptr<Accumulator::Node>> RamForest::roots() const
{
    std::vector<uint64_t> vRoots = this->state.rootPositions();
    auto rootPosIt = vRoots.begin();
    std::vector<std::shared_ptr<Accumulator::Node>> forestRoots;
    while (rootPosIt < vRoots.end()) {
        // TODO: check for nullptr returned from read.
        forestRoots.push_back(std::shared_ptr<Accumulator::Node>(
            (Accumulator::Node*)(new RamForest::Node(this->state, *rootPosIt, this->read(*rootPosIt)))));
        rootPosIt++;
    }

    return forestRoots;
}

std::vector<std::shared_ptr<Accumulator::Node>> RamForest::swapSubTrees(uint64_t posA, uint64_t posB) {}

std::shared_ptr<Accumulator::Node> RamForest::mergeRoot(uint64_t parentPos, uint256 parentHash)
{
    // compute row
    uint8_t row = this->state.detectRow(parentPos);

    // add hash to forest
    this->data.at(row).push_back(parentHash);

    return std::shared_ptr<Accumulator::Node>((Accumulator::Node*)new RamForest::Node(
        this->state,
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
        this->state,
        this->state.numLeaves,
        this->data.at(0).back()));
}
