#include "uint256.h"
#include <iostream>
#include <ram_forest.h>

// RamForest::Node
const uint256& RamForest::Node::Hash() const
{
    return this->m_hash;
}

void RamForest::Node::ReHash()
{
    // get the children hashes
    uint64_t left_child_pos = this->m_forest_state.Child(this->m_position, 0),
             right_child_pos = this->m_forest_state.Child(this->m_position, 1);
    uint256 left_child_hash = this->m_forest->Read(left_child_pos),
            right_child_hash = this->m_forest->Read(right_child_pos);

    // compute the hash
    this->m_hash = Accumulator::ParentHash(left_child_hash, right_child_hash);

    // write hash back
    uint8_t row = this->m_forest_state.DetectRow(this->m_position);
    uint64_t offset = this->m_forest_state.RowOffset(this->m_position);
    std::vector<uint256>& rowData = this->m_forest->m_data.at(row);
    rowData[this->m_position - offset] = this->m_hash;
}

NodePtr<Accumulator::Node> RamForest::Node::Parent() const
{
    uint64_t parent_pos = this->m_forest_state.Parent(this->m_position);
    uint8_t row = this->m_forest_state.DetectRow(this->m_position);
    bool row_has_root = this->m_forest_state.HasRoot(row);
    bool is_root = this->m_forest_state.RootPosition(row) == this->m_position;
    if (row_has_root && is_root) {
        return nullptr;
    }

    auto node = NodePtr<RamForest::Node>(m_forest->m_nodepool);
	node->m_forest_state = m_forest_state;
    node->m_forest = m_forest;
    node->m_position = parent_pos;

    return node;
}

// RamForest

const uint256 RamForest::Read(uint64_t pos) const
{
    uint8_t row = this->m_state.DetectRow(pos);
    uint64_t offset = this->m_state.RowOffset(pos);

    if (row >= this->m_data.size()) {
        // not enough rows
        std::cout << "not enough rows " << pos << std::endl;
        return uint256S("");
    }

    std::vector<uint256> rowData = this->m_data.at(row);

    if ((pos - offset) >= rowData.size()) {
        // row not big enough
        std::cout << "row not big enough " << pos << " " << offset << " " << +row << " " << rowData.size() << std::endl;
        return uint256S("");
    }

    return rowData.at(pos - offset);
}

void RamForest::SwapRange(uint64_t from, uint64_t to, uint64_t range)
{
    uint8_t row = this->m_state.DetectRow(from);
    uint64_t offset_from = this->m_state.RowOffset(from);
    uint64_t offset_to = this->m_state.RowOffset(to);
    std::vector<uint256>& rowData = this->m_data.at(row);

    for (uint64_t i = 0; i < range; ++i) {
        std::swap(rowData[(from - offset_from) + i], rowData[(to - offset_to) + i]);
    }
}

NodePtr<Accumulator::Node> RamForest::SwapSubTrees(uint64_t from, uint64_t to)
{
    // posA and posB are on the same row
    uint8_t row = this->m_state.DetectRow(from);
    from = this->m_state.LeftDescendant(from, row);
    to = this->m_state.LeftDescendant(to, row);

    for (uint64_t range = 1 << row; range != 0; range >>= 1) {
        this->SwapRange(from, to, range);
        from = this->m_state.Parent(from);
        to = this->m_state.Parent(to);
    }
    auto node = NodePtr<RamForest::Node>(m_nodepool);
	node->m_forest_state = m_state;
    node->m_forest = this;
    node->m_position = to;

    return node;
}

NodePtr<Accumulator::Node> RamForest::MergeRoot(uint64_t parent_pos, uint256 parent_hash)
{
    this->m_roots.pop_back();
    this->m_roots.pop_back();

    // compute row
    uint8_t row = this->m_state.DetectRow(parent_pos);

    // add hash to forest
    this->m_data.at(row).push_back(parent_hash);

    auto node = NodePtr<RamForest::Node>(m_nodepool);
	// TODO: should we set m_forest_state on the node?
    node->m_forest = this;
    node->m_position = parent_pos;
    node->m_hash = m_data.at(row).back();
    m_roots.push_back(node);

    return this->m_roots.back();
}

NodePtr<Accumulator::Node> RamForest::NewLeaf(uint256& hash)
{
    // append new hash on row 0 (as a leaf)
    this->m_data.at(0).push_back(hash);

    NodePtr<RamForest::Node> new_root(m_nodepool);
    new_root->m_forest = this;
    new_root->m_position = m_state.m_num_leaves;
    new_root->m_hash = hash;
    m_roots.push_back(new_root);
    //m_roots.emplace_back(std::move(new_root));

    return this->m_roots.back();
}

void RamForest::FinalizeRemove(const ForestState next_state)
{
    uint64_t num_leaves = next_state.m_num_leaves;
    // Go through each row and resize the row vectors for the next forest state.
    for (uint8_t row = 0; row < this->m_state.NumRows(); ++row) {
        this->m_data.at(row).resize(num_leaves);
        // Compute the number of nodes in the next row.
        num_leaves >>= 1;
    }

    // Compute the positions of the new roots in the current state.
    std::vector<uint64_t> new_positions = this->m_state.RootPositions(next_state.m_num_leaves);

    // Select the new roots.
    std::vector<NodePtr<Accumulator::Node>> new_roots;
    new_roots.reserve(new_positions.size());

    for (uint64_t new_pos : new_positions) {
        auto new_root = NodePtr<RamForest::Node>(m_nodepool);
        new_root->m_forest = this;
        new_root->m_position = new_pos;
        new_root->m_hash = this->Read(new_pos);
        new_roots.push_back(new_root);
    }

    this->m_roots = new_roots;
}

const Accumulator::BatchProof RamForest::Prove(const std::vector<uint64_t>& targets) const
{
    // TODO: check targets for validity like in remove.

    auto proof_positions = this->m_state.ProofPositions(targets);
    std::vector<uint256> proof;
    for (uint64_t pos : proof_positions.first) {
        proof.push_back(this->Read(pos));
    }

    return Accumulator::BatchProof(targets, proof);
}

void RamForest::Add(const std::vector<std::shared_ptr<Accumulator::Leaf>>& leaves)
{
    // Preallocate data with the required size.
    ForestState next_state(this->m_state.m_num_leaves + leaves.size());
    for (uint8_t row = 0; row < next_state.NumRows(); ++row) {
        if (row + 1 > this->m_data.size()) {
            this->m_data.push_back(std::vector<uint256>());
        }

        this->m_data.at(row).reserve(next_state.m_num_leaves >> row);
    }

    Accumulator::Add(leaves);
}
