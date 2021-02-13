#include "crypto/common.h"
#include <accumulator.h>
#include <check.h>
#include <crypto/sha512.h>
#include <cstring>
#include <iostream>
#include <stdio.h>

namespace utreexo {

bool Accumulator::Modify(const std::vector<Leaf>& leaves, const std::vector<uint64_t>& targets)
{
    if (!Remove(targets)) return false;
    // Addition can/should never fail.
    Add(leaves);

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

void Accumulator::PrintRoots() const
{
    for (auto root : m_roots) {
        std::cout << "root: " << root->m_position << ":" << HexStr(root->GetHash()) << std::endl;
    }
}

void Accumulator::Add(const std::vector<Leaf>& leaves)
{
    // Adding leaves can't be batched, so we add one by one.
    for (auto leaf = leaves.begin(); leaf < leaves.end(); ++leaf) {
        int root = m_roots.size() - 1;
        // Create a new leaf and append it to the end of roots.
        NodePtr<Accumulator::Node> new_root = this->NewLeaf(*leaf);

        // Merge the last two roots into one for every consecutive root from row 0 upwards.
        for (uint8_t row = 0; m_state.HasRoot(row); ++row) {
            Hash parent_hash;
            Accumulator::ParentHash(parent_hash, m_roots[root]->GetHash(), new_root->GetHash());
            new_root = MergeRoot(m_state.Parent(new_root->m_position), parent_hash);
            // Decreasing because we are going in reverse order.
            --root;
        }

        // Update the state by adding one leaf.
        uint8_t prev_rows = m_state.NumRows();
        m_state.Add(1);

        // Update the root positions.
        // This only needs to happen if the number of rows in the forest changes.
        // In this case there will always be exactly two roots, one on row 0 and one
        // on the next-to-last row.

        if (prev_rows == 0 || prev_rows == m_state.NumRows()) {
            continue;
        }

        assert(m_roots.size() >= 2);
        m_roots[1]->m_position = m_state.RootPosition(0);
        m_roots[0]->m_position = m_state.RootPosition(m_state.NumRows() - 1);
    }
}

bool IsSortedNoDupes(const std::vector<uint64_t>& targets)
{
    for (uint64_t i = 0; i < targets.size() - 1; ++i) {
        if (targets[i] >= targets[i + 1]) {
            return false;
        }
    }

    return true;
}

bool Accumulator::Remove(const std::vector<uint64_t>& targets)
{
    if (targets.size() == 0) {
        return true;
    }

    if (m_state.m_num_leaves < targets.size()) {
        // error deleting more targets than elemnts in the accumulator.
        return false;
    }

    if (!IsSortedNoDupes(targets)) {
        // error, targets are not sorted or contain duplicates.
        return false;
    }

    if (targets.back() >= m_state.m_num_leaves) {
        // error targets not in the accumulator.
        return false;
    }

    std::vector<std::vector<ForestState::Swap>> swaps = m_state.Transform(targets);
    // Store the nodes that have to be rehashed because their children changed.
    // These nodes are "dirty".
    std::vector<NodePtr<Accumulator::Node>> dirty_nodes;

    for (uint8_t row = 0; row < m_state.NumRows(); ++row) {
        std::vector<NodePtr<Accumulator::Node>> next_dirty_nodes;

        if (row < swaps.size()) {
            // Execute all the swaps in this row.
            for (const ForestState::Swap swap : swaps.at(row)) {
                NodePtr<Accumulator::Node> swap_dirt = SwapSubTrees(swap.m_from, swap.m_to);
                dirty_nodes.push_back(swap_dirt);
            }
        }

        // Rehash all the dirt after swapping.
        for (NodePtr<Accumulator::Node> dirt : dirty_nodes) {
            dirt->ReHash();
            if (next_dirty_nodes.size() == 0 || next_dirty_nodes.back()->m_position != m_state.Parent(dirt->m_position)) {
                NodePtr<Accumulator::Node> parent = dirt->Parent();
                if (parent) next_dirty_nodes.push_back(parent);
            }
        }

        dirty_nodes = next_dirty_nodes;
    }

    ForestState next_state(m_state.m_num_leaves - targets.size());
    FinalizeRemove(next_state);
    m_state.Remove(targets.size());

    return true;
}

// Accumulator::BatchProof
/*bool Accumulator::BatchProof::Verify(ForestState state, const std::vector<Hash> roots, const std::vector<Hash> target_hashes) const
{
    if (this->targets.size() != target_hashes.size()) {
        // TODO: error the number of targets does not math the number of provided hashes.
        return false;
    }

    std::vector<uint64_t> proof_positions, computable_positions;
    std::tie(proof_positions, computable_positions) = state.ProofPositions(this->targets);

    if (proof_positions.size() != this->proof.size()) {
        //TODO: error the number of proof hashes does not math the required number
        return false;
    }

    // target_nodes holds nodes that are known, on the bottom row those
    // are the targets, on the upper rows it holds computed nodes.
    std::vector<std::pair<uint64_t, Hash>> target_nodes;
    target_nodes.reserve(targets.size() * state.NumRows());
    for (uint64_t i = 0; i < this->targets.size(); ++i) {
        target_nodes.push_back(std::make_pair(this->targets[i], target_hashes[i]));
    }

    // root_candidates holds the roots that were computed and have to be
    // compared to the actual roots at the end.
    std::vector<Hash> root_candidates;
    root_candidates.reserve(roots.size());

    // Handle the row 0 root.
    if (state.HasRoot(0) && this->targets.back() == state.RootPosition(0)) {
        root_candidates.push_back(target_nodes.back().second);
        target_nodes.pop_back();
    }

    uint64_t proof_index = 0;
    for (uint64_t target_index = 0; target_index < target_nodes.size();) {
        std::pair<uint64_t, Hash> target = target_nodes[target_index], proof;

        // Find the proof node. It will either be in the batch proof or in target_nodes.
        if (proof_index < proof_positions.size() && state.Sibling(target.first) == proof_positions[proof_index]) {
            // target has its sibling in the proof.
            proof = std::make_pair(proof_positions[proof_index], this->proof[proof_index]);
            ++proof_index;
            ++target_index;
        } else {
            if (target_index + 1 >= target_nodes.size()) {
                // TODO: error the sibling was expected to be in the targets but it was not.
                return false;
            }
            // target has its sibling in the targets.
            proof = target_nodes[target_index + 1];
            // Advance by two because both the target and the proof where found in target_nodes.
            target_index += 2;
        }

        auto left = target, right = proof;
        if (state.RightSibling(left.first) == left.first) {
            // Left was actually right and right was actually left.
            std::swap(left, right);
        }

        // Compute the parent hash.
        uint64_t parent_pos = state.Parent(left.first);
        Hash parent_hash = Accumulator::ParentHash(left.second, right.second);

        uint64_t parent_row = state.DetectRow(parent_pos);
        if (state.HasRoot(parent_row) && parent_pos == state.RootPosition(parent_row)) {
            // Store the parent as a root candidate.
            root_candidates.push_back(parent_hash);
            continue;
        }

        target_nodes.push_back(std::make_pair(parent_pos, parent_hash));
    }

    if (root_candidates.size() == 0) {
        // TODO: error no roots to verify
        return false;
    }

    uint8_t rootMatches = 0;
    for (Hash root : roots) {
        if (root_candidates.size() > rootMatches && root.Compare(root_candidates[rootMatches]) == 0) {
            ++rootMatches;
        }
    }

    if (rootMatches != root_candidates.size()) {
        // TODO: error not all roots matched.
        return false;
    }

    return true;
}*/

void Accumulator::BatchProof::Serialize(std::vector<uint8_t>& bytes) const
{
    // Number of targets:       4 bytes
    // Number of proof hashes:  4 bytes
    // Targets:                 4 bytes each
    // Proof hashes:           32 bytes each
    bytes.resize(4 + 4 + targets.size() * 4 + proof.size() * 32);

    int data_offset = 0;
    WriteBE32(bytes.data(), uint32_t(targets.size()));
    data_offset += 4;
    WriteBE32(bytes.data() + data_offset, uint32_t(proof.size()));
    data_offset += 4;

    for (const uint64_t target : targets) {
        WriteBE32(bytes.data() + data_offset, uint32_t(target));
        data_offset += 4;
    }

    for (const Hash& hash : proof) {
        std::memcpy(bytes.data() + data_offset, hash.data(), 32);
        data_offset += 32;
    }
}

bool Accumulator::BatchProof::Unserialize(const std::vector<uint8_t>& bytes)
{
    if (bytes.size() < 8) {
        // 8 byte minimum for the number of targets and proof hashses
        return false;
    }

    int data_offset = 0;
    uint32_t num_targets = ReadBE32(bytes.data());
    data_offset += 4;
    uint32_t num_hashes = ReadBE32(bytes.data() + data_offset);
    data_offset += 4;

    if (bytes.size() != 8 + num_targets * 4 + num_hashes * 32) {
        return false;
    }

    targets.clear();
    proof.clear();
    targets.reserve(num_targets);
    proof.reserve(num_hashes);

    for (uint32_t i = 0; i < num_targets; ++i) {
        targets.push_back(uint64_t(ReadBE32(bytes.data() + data_offset)));
        data_offset += 4;
    }

    for (uint32_t i = 0; i < num_hashes; ++i) {
        Hash hash;
        std::memcpy(hash.data(), bytes.data() + data_offset, 32);
        data_offset += 32;
        proof.push_back(hash);
    }

    assert(data_offset == bytes.size());

    return true;
}

bool Accumulator::BatchProof::operator==(const BatchProof& other)
{
    return targets.size() == other.targets.size() && proof.size() == other.proof.size() &&
           targets == other.targets && proof == other.proof;
}

void Accumulator::BatchProof::Print()
{
    std::cout << "targets: ";
    print_vector(this->targets);

    std::cout << "proof: ";
    for (const Hash& hash : proof) {
        std::cout << HexStr(hash) << ", ";
    }

    std::cout << std::endl;
}

}; // namespace utreexo
