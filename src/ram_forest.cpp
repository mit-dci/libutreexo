#include "../include/ram_forest.h"
#include "../include/batchproof.h"
#include "crypto/common.h"
#include "node.h"
#include "state.h"
#include <algorithm>
#include <iostream>

namespace utreexo {

size_t RamForest::LeafHasher::operator()(const Hash& hash) const { return ReadLE64(hash.data()); }

class RamForest::Node : public Accumulator::Node
{
public:
    Hash m_hash;
    // TODO: yikes.
    RamForest* m_forest;

    Node() {}

    const Hash& GetHash() const override;
    void ReHash() override;
    NodePtr<Accumulator::Node> Parent() const override;
};

// RamForest::Node
const Hash& RamForest::Node::GetHash() const
{
    return this->m_hash;
}

void RamForest::Node::ReHash()
{
    ForestState state(m_num_leaves);
    // get the children hashes
    uint64_t left_child_pos = state.Child(this->m_position, 0),
             right_child_pos = state.Child(this->m_position, 1);
    Hash left_child_hash, right_child_hash;
    bool ok = m_forest->Read(left_child_hash, left_child_pos);
    assert(ok);
    ok = m_forest->Read(right_child_hash, right_child_pos);
    assert(ok);

    // compute the hash
    Accumulator::ParentHash(m_hash, left_child_hash, right_child_hash);

    // write hash back
    uint8_t row = state.DetectRow(this->m_position);
    uint64_t offset = state.RowOffset(this->m_position);
    std::vector<Hash>& rowData = this->m_forest->m_data.at(row);
    rowData[this->m_position - offset] = this->m_hash;
}

Accumulator::NodePtr<Accumulator::Node> RamForest::Node::Parent() const
{
    ForestState state(m_num_leaves);
    uint64_t parent_pos = state.Parent(this->m_position);

    // Check if this node is a root.
    // If so return nullptr becauce roots do not have parents.
    uint8_t row = state.DetectRow(this->m_position);
    bool row_has_root = state.HasRoot(row);
    bool is_root = state.RootPosition(row) == this->m_position;
    if (row_has_root && is_root) {
        return nullptr;
    }

    // Return the parent of this node.
    auto node = NodePtr<RamForest::Node>(m_forest->m_nodepool);
    node->m_num_leaves = m_num_leaves;
    node->m_forest = m_forest;
    node->m_position = parent_pos;
    return node;
}

// RamForest

RamForest::RamForest(uint64_t num_leaves, int max_nodes) : Accumulator(num_leaves)
{
    this->m_data = std::vector<std::vector<Hash>>();
    this->m_data.push_back(std::vector<Hash>());
    m_nodepool = new NodePool<Node>(max_nodes);
}

bool RamForest::Read(Hash& hash, uint64_t pos) const
{
    ForestState current_state = ForestState(m_num_leaves);
    uint8_t row = current_state.DetectRow(pos);
    uint64_t offset = current_state.RowOffset(pos);

    if (row >= this->m_data.size()) {
        // not enough rows
        std::cout << "not enough rows " << pos << std::endl;
        return false;
    }

    std::vector<Hash> rowData = this->m_data.at(row);

    if ((pos - offset) >= rowData.size()) {
        // row not big enough
        std::cout << "row not big enough " << pos << " " << offset << " " << +row << " " << rowData.size() << std::endl;
        return false;
    }

    hash = rowData.at(pos - offset);
    return true;
}

void RamForest::SwapRange(uint64_t from, uint64_t to, uint64_t range)
{
    ForestState current_state = ForestState(m_num_leaves);
    uint8_t row = current_state.DetectRow(from);
    uint64_t offset_from = current_state.RowOffset(from);
    uint64_t offset_to = current_state.RowOffset(to);
    std::vector<Hash>& rowData = m_data.at(row);

    for (uint64_t i = 0; i < range; ++i) {
        std::swap(rowData[(from - offset_from) + i], rowData[(to - offset_to) + i]);

        // Swap postions in the position map if we are on the bottom.
        if (row == 0) std::swap(m_posmap[rowData[(from - offset_from) + i]], m_posmap[rowData[(to - offset_to) + i]]);
    }
}

Accumulator::NodePtr<Accumulator::Node> RamForest::SwapSubTrees(uint64_t from, uint64_t to)
{
    ForestState current_state(m_num_leaves);
    // posA and posB are on the same row
    uint8_t row = current_state.DetectRow(from);
    assert(row == current_state.DetectRow(to));

    from = current_state.LeftDescendant(from, row);
    to = current_state.LeftDescendant(to, row);

    for (uint64_t range = 1 << row; range != 0; range >>= 1) {
        this->SwapRange(from, to, range);
        from = current_state.Parent(from);
        to = current_state.Parent(to);
    }

    auto node = NodePtr<RamForest::Node>(m_nodepool);
    node->m_num_leaves = m_num_leaves;
    node->m_forest = this;
    node->m_position = to;

    return node;
}

Accumulator::NodePtr<Accumulator::Node> RamForest::MergeRoot(uint64_t parent_pos, Hash parent_hash)
{
    assert(m_roots.size() >= 2);

    m_roots.pop_back();
    m_roots.pop_back();
    // compute row
    uint8_t row = ForestState(m_num_leaves).DetectRow(parent_pos);
    assert(m_data.size() > row);

    // add hash to forest
    m_data.at(row).push_back(parent_hash);

    auto node = NodePtr<RamForest::Node>(m_nodepool);
    // TODO: should we set m_forest_state on the node?
    node->m_num_leaves = m_num_leaves;
    node->m_forest = this;
    node->m_position = parent_pos;
    node->m_hash = m_data.at(row).back();
    m_roots.push_back(node);

    return m_roots.back();
}

Accumulator::NodePtr<Accumulator::Node> RamForest::NewLeaf(const Leaf& leaf)
{
    // append new hash on row 0 (as a leaf)
    this->m_data.at(0).push_back(leaf.first);

    NodePtr<RamForest::Node> new_root(m_nodepool);
    new_root->m_num_leaves = m_num_leaves;
    new_root->m_forest = this;
    new_root->m_position = m_num_leaves;
    new_root->m_hash = leaf.first;
    m_roots.push_back(new_root);

    m_posmap[leaf.first] = new_root->m_position;

    return this->m_roots.back();
}

void RamForest::FinalizeRemove(uint64_t next_num_leaves)
{
    ForestState current_state(m_num_leaves), next_state(next_num_leaves);

    assert(next_state.m_num_leaves <= current_state.m_num_leaves);

    // Remove deleted leaf hashes from the position map.
    for (uint64_t pos = next_state.m_num_leaves; pos < current_state.m_num_leaves; ++pos) {
        Hash to_erase;

        bool ok = Read(to_erase, pos);
        assert(ok);

        m_posmap.erase(to_erase);
    }

    uint64_t num_leaves = next_state.m_num_leaves;
    // Go through each row and resize the row vectors for the next forest state.
    for (uint8_t row = 0; row < current_state.NumRows(); ++row) {
        this->m_data.at(row).resize(num_leaves);
        // Compute the number of nodes in the next row.
        num_leaves >>= 1;
    }

    // Compute the positions of the new roots in the current state.
    std::vector<uint64_t> new_positions = current_state.RootPositions(next_state.m_num_leaves);

    // Select the new roots.
    std::vector<NodePtr<Accumulator::Node>> new_roots;
    new_roots.reserve(new_positions.size());

    for (uint64_t new_pos : new_positions) {
        auto new_root = NodePtr<RamForest::Node>(m_nodepool);
        new_root->m_forest = this;
        new_root->m_position = new_pos;
        bool ok = Read(new_root->m_hash, new_pos);
        assert(ok);
        new_roots.push_back(new_root);
    }

    this->m_roots = new_roots;
}

bool RamForest::Prove(BatchProof& proof, const std::vector<Hash>& targetHashes) const
{
    // Figure out the positions of the target hashes via the position map.
    std::vector<uint64_t> targets;
    targets.reserve(targetHashes.size());
    for (const Hash& hash : targetHashes) {
        auto posmap_it = m_posmap.find(hash);
        if (posmap_it == m_posmap.end()) {
            // TODO: error
            return false;
        }
        targets.push_back(posmap_it->second);
    }

    // We need the sorted targets to compute the proof positions.
    std::vector<uint64_t> sorted_targets(targets);
    std::sort(sorted_targets.begin(), sorted_targets.end());

    assert(ForestState(m_num_leaves).CheckTargetsSanity(sorted_targets));

    // Read proof hashes from the forest using the proof positions
    auto proof_positions = ForestState(m_num_leaves).ProofPositions(sorted_targets);
    std::vector<Hash> proof_hashes(proof_positions.first.size());
    for (int i = 0; i < proof_hashes.size(); i++) {
        bool ok = Read(proof_hashes[i], proof_positions.first[i]);
        assert(ok);
    }

    // Create the batch proof from the *unsorted* targets and the proof hashes.
    proof = BatchProof(targets, proof_hashes);
    return true;
}

bool RamForest::Verify(const BatchProof& proof, const std::vector<Hash>& target_hashes)
{
    // TODO: verify the actual proof. a bitcoin bridge node would like to validate proofs to ensure
    // that he does not give invalid proofs to anyone.
    // For now just check that the target hashes exist.
    for (const Hash& hash : target_hashes) {
        auto it = m_posmap.find(hash);
        if (it == m_posmap.end()) return false;
    }

    return true;
}

void RamForest::Add(const std::vector<Leaf>& leaves)
{
    // Preallocate data with the required size.
    ForestState next_state(m_num_leaves + leaves.size());
    for (uint8_t row = 0; row <= next_state.NumRows(); ++row) {
        if (row >= this->m_data.size()) {
            m_data.push_back(std::vector<Hash>());
        }

        m_data.at(row).reserve(next_state.m_num_leaves >> row);
    }
    assert(m_data.size() > next_state.NumRows());

    Accumulator::Add(leaves);

    assert(next_state.m_num_leaves == m_num_leaves);
    assert(m_posmap.size() == m_num_leaves);
}

}; // namespace utreexo
