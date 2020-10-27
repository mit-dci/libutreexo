#include <accumulator.h>
#include <crypto/sha256.h>
#include <iostream>
#include <stdio.h>
#include <uint256.h>

void Accumulator::modify(const std::vector<std::shared_ptr<Accumulator::Leaf>>& leaves, const std::vector<uint64_t>& targets)
{
    this->remove(targets);
    this->add(leaves);
}

uint256 Accumulator::parentHash(const uint256& left, const uint256& right)
{
    CSHA256 hasher;

    // copy the two hashes into one 64 byte buffer
    uint8_t data[64];
    memcpy(data, left.begin(), 32);
    memcpy(data + 32, right.begin(), 32);
    hasher.Write(data, 64);

    // finalize the hash and write it into parentHash
    uint256 parentHash;
    hasher.Finalize(parentHash.begin());

    return parentHash;
}

void Accumulator::printRoots(const std::vector<std::shared_ptr<Accumulator::Node>>& roots) const
{
    for (auto root : roots) {
        std::cout << "root: " << root->position << ":" << root->hash().GetHex() << std::endl;
    }
}

void Accumulator::add(const std::vector<std::shared_ptr<Accumulator::Leaf>>& leaves)
{
    for (auto leaf = leaves.begin(); leaf < leaves.end(); leaf++) {
        std::vector<std::shared_ptr<Accumulator::Node>> roots = this->roots();

        auto root = roots.rbegin();
        std::shared_ptr<Accumulator::Node> newRoot = this->newLeaf((*leaf)->hash());
        for (uint8_t row = 0; this->state.hasRoot(row); row++) {
            uint64_t parentPos = this->state.parent(newRoot->position);
            uint256 hash = Accumulator::parentHash((*root)->hash(), newRoot->hash());
            newRoot = this->mergeRoot(parentPos, hash);
            root++;
        }
        this->state.add(1);
    }

    std::cout << "roots after adding:" << std::endl;
    std::vector<std::shared_ptr<Accumulator::Node>> roots = this->roots();
    printRoots(roots);
}

bool isSortedNoDupes(const std::vector<uint64_t>& targets)
{
    return true;
}

void Accumulator::remove(const std::vector<uint64_t>& targets)
{
    if (targets.size() == 0) {
        return;
    }

    if (this->state.numLeaves < targets.size()) {
        // TODO: error deleting more targets than elemnts in the accumulator.
        return;
    }

    if (!isSortedNoDupes(targets)) {
        // TODO: error targets are not sorted or contain duplicates.
        return;
    }

    if (targets.back() >= this->state.numLeaves) {
        // TODO: error targets not in the accumulator.
        return;
    }

    std::vector<std::vector<ForestState::Swap>> swaps = this->state.transform(targets);
    // Store the nodes that have to be rehashed because their children changed.
    // These nodes are "dirty".
    std::vector<std::shared_ptr<Accumulator::Node>> dirtyNodes;

    for (uint8_t row = 0; row < this->state.numRows(); row++) {
        std::vector<std::shared_ptr<Accumulator::Node>> nextDirtyNodes;
        if (row < swaps.size()) {
            // Execute all the swaps in this row.
            for (const ForestState::Swap swap : swaps.at(row)) {
                std::shared_ptr<Accumulator::Node> swapDirt = this->swapSubTrees(swap.from, swap.to);
                dirtyNodes.push_back(swapDirt);
            }
        }

        // Rehash all the dirt after swapping.
        for (std::shared_ptr<Accumulator::Node> dirt : dirtyNodes) {
            dirt->reHash();
            std::shared_ptr<Accumulator::Node> parent = dirt->parent();
            if (parent && (nextDirtyNodes.size() == 0 || nextDirtyNodes.back()->position != parent->position)) {
                nextDirtyNodes.push_back(parent);
            }
        }

        dirtyNodes = nextDirtyNodes;
    }

    ForestState nextState(this->state.numLeaves - targets.size());
    this->finalizeRemove(nextState);
    this->state.remove(targets.size());
}
