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

const std::vector<uint256> Accumulator::roots() const
{
    std::vector<uint256> result;
    result.reserve(this->mRoots.size());

    for (auto root : this->mRoots) {
        result.push_back(root->hash());
    }

    return result;
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
        int root = this->mRoots.size() - 1;
        std::shared_ptr<Accumulator::Node> newRoot = this->newLeaf((*leaf)->hash());

        for (uint8_t row = 0; this->state.hasRoot(row); row++) {
            uint64_t parentPos = this->state.parent(newRoot->position);
            uint256 hash = Accumulator::parentHash(this->mRoots[root]->hash(), newRoot->hash());
            newRoot = this->mergeRoot(parentPos, hash);
            // Decreasing because we are going in reverse order.
            root--;
        }

        // Update the state by adding one leaf.
        uint8_t prevRows = this->state.numRows();
        this->state.add(1);
        if (prevRows == 0 || prevRows == this->state.numRows()) {
            continue;
        }

        // Update the root positions.
        // This only need happen if the number of rows in the forest changes.
        // In this case there will always be exactly two roots, one on row 0 and one
        // on the next-to-last row.
        this->mRoots[1]->position = this->state.rootPosition(0);
        this->mRoots[0]->position = this->state.rootPosition(this->state.numRows() - 1);
    }

    std::cout << "roots after adding:" << std::endl;
    print_vector(this->state.rootPositions());
    printRoots(this->mRoots);
}

bool isSortedNoDupes(const std::vector<uint64_t>& targets)
{
    for (uint64_t i = 0; i < targets.size() - 1; i++) {
        if (targets[i] >= targets[i + 1]) {
            return false;
        }
    }

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

// Accumulator::BatchProof
bool Accumulator::BatchProof::verify(ForestState state, const std::vector<uint256> roots, const std::vector<uint256> targetHashes) const
{
    if (this->targets.size() != targetHashes.size()) {
        // TODO: error the number of targets does not math the number of provided hashes.
        return false;
    }

    std::vector<uint64_t> proofPositions, computablePositions;
    std::tie(proofPositions, computablePositions) = state.proofPositions(this->targets);

    if (proofPositions.size() != this->proof.size()) {
        //TODO: error the number of proof hashes does not math the required number
        return false;
    }

    // targetNodes holds nodes that are known, on the bottom row those
    // are the targets, on the upper rows it holds computed nodes.
    std::vector<std::pair<uint64_t, uint256>> targetNodes;
    targetNodes.reserve(targets.size() * state.numRows());
    for (uint64_t i = 0; i < this->targets.size(); i++) {
        targetNodes.push_back(std::make_pair(this->targets[i], targetHashes[i]));
    }

    // rootCandidates holds the roots that were computed and have to be
    // compared to the actual roots at the end.
    std::vector<uint256> rootCandidates;
    rootCandidates.reserve(roots.size());

    // Handle the row 0 root.
    if (state.hasRoot(0) && this->targets.back() == state.rootPosition(0)) {
        rootCandidates.push_back(targetNodes.back().second);
        targetNodes.pop_back();
    }

    uint64_t proofIndex = 0;
    for (uint64_t targetIndex = 0; targetIndex < targetNodes.size();) {
        std::pair<uint64_t, uint256> target = targetNodes[targetIndex], proof;

        // Find the proof node. It will either be in the batch proof or in targetNodes.
        if (proofIndex < proofPositions.size() && state.sibling(target.first) == proofPositions[proofIndex]) {
            // target has its sibling in the proof.
            proof = std::make_pair(proofPositions[proofIndex], this->proof[proofIndex]);
            proofIndex++;
            targetIndex++;
        } else {
            if (targetIndex + 1 >= targetNodes.size()) {
                // TODO: error the sibling was expected to be in the targets but it was not.
                return false;
            }
            // target has its sibling in the targets.
            proof = targetNodes[targetIndex + 1];
            // Advance by two because both the target and the proof where found in targetNodes.
            targetIndex += 2;
        }

        auto left = target, right = proof;
        if (state.rightSibling(left.first) == left.first) {
            // Left was actually right and right was actually left.
            std::swap(left, right);
        }

        // Compute the parent hash.
        uint64_t parentPos = state.parent(left.first);
        uint256 parentHash = Accumulator::parentHash(left.second, right.second);

        uint64_t parentRow = state.detectRow(parentPos);
        if (state.hasRoot(parentRow) && parentPos == state.rootPosition(parentRow)) {
            // Store the parent as a root candidate.
            rootCandidates.push_back(parentHash);
            continue;
        }

        targetNodes.push_back(std::make_pair(parentPos, parentHash));
    }

    if (rootCandidates.size() == 0) {
        // TODO: error no roots to verify
        return false;
    }

    uint8_t rootMatches = 0;
    for (uint256 root : roots) {
        if (rootCandidates.size() > rootMatches && root.Compare(rootCandidates[rootMatches]) == 0) {
            rootMatches++;
        }
    }

    if (rootMatches != rootCandidates.size()) {
        // TODO: error not all roots matched.
        return false;
    }

    return true;
}

void Accumulator::BatchProof::print()
{
    std::cout << "targets: ";
    print_vector(this->targets);

    std::cout << "proof: ";
    for (int i = 0; i < this->proof.size(); i++) {
        std::cout << proof[i].GetHex().substr(60, 64) << ", ";
    }

    std::cout << std::endl;
}
