#include "check.h"
#include "crypto/common.h"
#include "crypto/sha512.h"
#include "include/accumulator.h"
#include "node.h"
#include "state.h"

#include <cstring>
#include <iostream>
#include <stdio.h>

#include "state.h"

namespace utreexo {

Accumulator::Accumulator(uint64_t num_leaves)
{
    m_num_leaves = num_leaves;
    m_roots.reserve(64);
}

Accumulator::~Accumulator() {}

std::optional<const Hash> Accumulator::Read(uint64_t pos) const
{
    return Read(ForestState(m_num_leaves), pos);
}

bool Accumulator::Modify(const std::vector<Leaf>& leaves, const std::vector<uint64_t>& targets)
{
    if (!Remove(targets)) return false;
    if (!Add(leaves)) return false;

    return true;
}

void Accumulator::Roots(std::vector<Hash>& roots) const
{
    roots.clear();
    roots.reserve(m_roots.size());

    for (auto root : m_roots) {
        roots.push_back(root->GetHash());
    }
}

uint64_t Accumulator::NumLeaves() const
{
    return m_num_leaves;
}

// https://github.com/bitcoin/bitcoin/blob/7f653c3b22f0a5267822eec017aea6a16752c597/src/util/strencodings.cpp#L580
template <class T>
std::string HexStr(const T s)
{
    std::string rv;
    static constexpr char hexmap[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    rv.reserve(s.size() * 2);
    for (uint8_t v : s) {
        rv.push_back(hexmap[v >> 4]);
        rv.push_back(hexmap[v & 15]);
    }
    return rv;
}

void Accumulator::ParentHash(Hash& parent, const Hash& left, const Hash& right)
{
    //CHECK_SAFE(!left.IsNull());
    //CHECK_SAFE(!right.IsNull());

    CSHA512 hasher(CSHA512::OUTPUT_SIZE_256);

    // copy the two hashes into one 64 byte buffer
    uint8_t data[64];
    memcpy(data, left.data(), 32);
    memcpy(data + 32, right.data(), 32);
    hasher.Write(data, 64);

    // finalize the hash and write it into parentHash
    hasher.Finalize256(parent.data());

    // std::cout << "hash: " << HexStr(left).substr(0, 8) << " + " << HexStr(right).substr(0, 8) << " = " << HexStr(parent).substr(0, 8) << std::endl;
}

void Accumulator::PrintRoots() const
{
    for (auto root : m_roots) {
        std::cout << "root: " << root->m_position << ":" << HexStr(root->GetHash()) << std::endl;
    }
}

bool Accumulator::Add(const std::vector<Leaf>& leaves)
{
    ForestState current_state(m_num_leaves);
    // TODO Adding leaves can be batched. Do implement this later.
    for (auto leaf = leaves.begin(); leaf < leaves.end(); ++leaf) {
        int root = m_roots.size() - 1;
        // Create a new leaf and append it to the end of roots.
        NodePtr<Accumulator::Node> new_root = this->NewLeaf(*leaf);

        // Merge the last two roots into one for every consecutive root from row 0 upwards.
        for (uint8_t row = 0; current_state.HasRoot(row); ++row) {
            Hash parent_hash;
            Accumulator::ParentHash(parent_hash, m_roots[root]->GetHash(), new_root->GetHash());
            new_root = MergeRoot(current_state.Parent(new_root->m_position), parent_hash);
            // Decreasing because we are going in reverse order.
            --root;
        }

        uint8_t prev_rows = current_state.NumRows();

        ++m_num_leaves;
        current_state = ForestState(m_num_leaves);

        // Update the root positions.
        // This only needs to happen if the number of rows in the forest changes.
        // In this case there will always be exactly two roots, one on row 0 and one
        // on the next-to-last row.

        if (prev_rows == 0 || prev_rows == current_state.NumRows()) {
            continue;
        }

        assert(m_roots.size() >= 2);
        m_roots[1]->m_position = current_state.RootPosition(0);
        m_roots[0]->m_position = current_state.RootPosition(current_state.NumRows() - 1);
    }

    return true;
}

bool Accumulator::Remove(const std::vector<uint64_t>& targets)
{
    if (targets.size() == 0) {
        return true;
    }

    ForestState current_state(m_num_leaves);

    // Perform sanity checks on targets. (e.g.: sorted no duplicates)
    if (!current_state.CheckTargetsSanity(targets)) {
        return false;
    }

    std::vector<std::vector<ForestState::Swap>> swaps = current_state.Transform(targets);
    // Store the nodes that have to be rehashed because their children changed.
    // These nodes are "dirty".
    std::vector<NodePtr<Accumulator::Node>> dirty_nodes;

    for (uint8_t row = 0; row < current_state.NumRows(); ++row) {
        std::vector<NodePtr<Accumulator::Node>> next_dirty_nodes;

        if (row < swaps.size()) {
            // Execute all the swaps in this row.
            for (const ForestState::Swap swap : swaps.at(row)) {
                NodePtr<Accumulator::Node> swap_dirt = SwapSubTrees(swap.m_from, swap.m_to);
                if (!swap.m_collapse) dirty_nodes.push_back(swap_dirt);
            }
        }

        // Rehash all the dirt after swapping.
        for (NodePtr<Accumulator::Node> dirt : dirty_nodes) {
            dirt->ReHash();
            if (next_dirty_nodes.size() == 0 || next_dirty_nodes.back()->m_position != current_state.Parent(dirt->m_position)) {
                NodePtr<Accumulator::Node> parent = dirt->Parent();
                if (parent) next_dirty_nodes.push_back(parent);
            }
        }

        dirty_nodes = next_dirty_nodes;
    }

    assert(dirty_nodes.size() == 0);

    uint64_t next_num_leaves = m_num_leaves - targets.size();
    FinalizeRemove(next_num_leaves);
    m_num_leaves = next_num_leaves;

    return true;
}

bool Accumulator::Prove(BatchProof& proof, const std::vector<Hash>& target_hashes) const
{
    // Figure out the positions of the target hashes via the position map.
    std::vector<uint64_t> targets;
    targets.reserve(target_hashes.size());
    for (const Hash& hash : target_hashes) {
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
        auto hash = Read(proof_positions.first[i]);
        if (hash) {
            proof_hashes[i] = hash.value();
        }
    }

    // Create the batch proof from the *unsorted* targets and the proof hashes.
    proof = BatchProof(targets, proof_hashes);
    return true;
}

}; // namespace utreexo
