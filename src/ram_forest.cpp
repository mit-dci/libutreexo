#include "../include/ram_forest.h"
#include "../include/batchproof.h"
#include "check.h"
#include "crypto/common.h"
#include "node.h"
#include "state.h"
#include <algorithm>
#include <iostream>
#include <unordered_set>

namespace utreexo {

size_t RamForest::LeafHasher::operator()(const Hash& hash) const { return ReadLE64(hash.data()); }

class RamForest::Node : public Accumulator::Node
{
public:
    Hash m_hash;
    // TODO: yikes.
    RamForest* m_forest;

    Node() {}
    Node(RamForest* forest,
         uint64_t num_leaves,
         uint64_t pos)
        : m_forest(forest)
    {
        m_num_leaves = num_leaves;
        m_position = pos;
    }
    Node(RamForest* forest,
         const Hash& hash,
         uint64_t num_leaves,
         uint64_t pos)
        : RamForest::Node(forest, num_leaves, pos)
    {
        m_hash = hash;
    }

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
    const Hash& left_child_hash = m_forest->Read(left_child_pos);
    const Hash& right_child_hash = m_forest->Read(right_child_pos);

    // compute the hash
    Accumulator::ParentHash(m_hash, left_child_hash, right_child_hash);

    // write hash back
    uint8_t row = state.DetectRow(m_position);
    uint64_t offset = state.RowOffset(m_position);
    std::vector<Hash>& rowData = m_forest->m_data.at(row);
    rowData[m_position - offset] = m_hash;
}

NodePtr<Accumulator::Node> RamForest::Node::Parent() const
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
    return Accumulator::MakeNodePtr<RamForest::Node>(m_forest, m_num_leaves, parent_pos);
}

// RamForest

RamForest::RamForest(uint64_t num_leaves) : Accumulator(num_leaves)
{
    this->m_data = std::vector<std::vector<Hash>>();
    this->m_data.push_back(std::vector<Hash>());
}

RamForest::RamForest(const std::string& file) : Accumulator(0)
{
    this->m_data = std::vector<std::vector<Hash>>();
    this->m_data.push_back(std::vector<Hash>());
    m_file_path = file;

    if (static_cast<bool>(std::fstream(file))) {
        // We can restore the forest from an existing file.
        m_file = std::fstream(file,
                              std::fstream::in | std::fstream::out | std::fstream::binary);
        Restore();
    } else {
        m_num_leaves = 0;
        m_file = std::fstream(file,
                              std::fstream::in | std::fstream::out | std::fstream::binary | std::fstream::trunc);
        Commit();
    }
}

RamForest::~RamForest()
{
    if (m_file.good()) {
        Commit();
        m_file.flush();
        m_file.close();
    }
}

bool RamForest::Restore()
{
    char uint64_buf[8];
    m_file.seekg(0);

    // restore number of leaves
    m_file.read(reinterpret_cast<char*>(uint64_buf), 8);
    m_num_leaves = ReadBE64(reinterpret_cast<const uint8_t*>(uint64_buf));

    ForestState state(m_num_leaves);
    // restore forest hashes
    uint64_t num_hashes = m_num_leaves;
    uint8_t row = 0;
    uint64_t pos = 0;
    while (num_hashes > 0) {
        pos = state.RowOffset(row);
        for (uint64_t i = 0; i < num_hashes; ++i) {
            Hash hash;
            m_file.read(reinterpret_cast<char*>(hash.data()), 32);
            m_data[row].push_back(hash);

            if (num_hashes == m_num_leaves) {
                // populate position map
                m_posmap[hash] = pos;
            }
            ++pos;
        }

        m_data.push_back({});
        row++;
        num_hashes >>= 1;
    }

    RestoreRoots();

    return true;
}

bool RamForest::Commit()
{
    char uint64_buf[8];
    m_file.seekg(0);

    // commit number of leaves
    WriteBE64(reinterpret_cast<uint8_t*>(uint64_buf), m_num_leaves);
    m_file.write(reinterpret_cast<char*>(uint64_buf), 8);

    // commit forest hashes
    ForestState state(m_num_leaves);
    uint64_t num_hashes = m_num_leaves;
    for (uint8_t i = 0; i <= state.NumRows(); ++i) {
        assert(num_hashes <= m_data[i].size());
        for (int j = 0; j < num_hashes; ++j) {
            m_file.write(reinterpret_cast<const char*>(m_data[i][j].data()), 32);
        }
        num_hashes >>= 1;
    }

    return true;
}

const Hash& RamForest::Read(ForestState state, uint64_t pos) const
{
    uint8_t row = state.DetectRow(pos);
    uint64_t offset = state.RowOffset(pos);

    assert(row < m_data.size());

    const std::vector<Hash>& row_data = m_data.at(row);

    assert((pos - offset) < row_data.size());

    return row_data.at(pos - offset);
}

const Hash& RamForest::Read(uint64_t pos) const
{
    ForestState state(m_num_leaves);
    return Read(state, pos);
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

NodePtr<Accumulator::Node> RamForest::SwapSubTrees(uint64_t from, uint64_t to)
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

    return Accumulator::MakeNodePtr<RamForest::Node>(this, m_num_leaves, to);
}

NodePtr<Accumulator::Node> RamForest::MergeRoot(uint64_t parent_pos, Hash parent_hash)
{
    assert(m_roots.size() >= 2);

    m_roots.pop_back();
    m_roots.pop_back();
    // compute row
    ForestState state(m_num_leaves);
    uint8_t row = state.DetectRow(parent_pos);
    assert(m_data.size() > row);

    // add hash to forest
    m_data.at(row).push_back(parent_hash);
    uint64_t offset = state.RowOffset(parent_pos);
    m_data[row][parent_pos - offset] = parent_hash;

    NodePtr<RamForest::Node> node = Accumulator::MakeNodePtr<RamForest::Node>(this, m_data.at(row).back(), m_num_leaves, parent_pos);
    m_roots.push_back(node);

    return m_roots.back();
}

NodePtr<Accumulator::Node> RamForest::NewLeaf(const Leaf& leaf)
{
    // append new hash on row 0 (as a leaf)
    this->m_data[0][m_num_leaves] = leaf.first;

    NodePtr<RamForest::Node> new_root = Accumulator::MakeNodePtr<RamForest::Node>(this, leaf.first, m_num_leaves, m_num_leaves);
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
        m_posmap.erase(Read(pos));
    }

    assert(m_posmap.size() == next_num_leaves);

    // Compute the positions of the new roots in the current state.
    std::vector<uint64_t> new_positions = current_state.RootPositions(next_state.m_num_leaves);

    // Select the new roots.
    std::vector<NodePtr<Accumulator::Node>> new_roots;
    new_roots.reserve(new_positions.size());

    for (uint64_t new_pos : new_positions) {
        NodePtr<RamForest::Node> new_root = Accumulator::MakeNodePtr<RamForest::Node>(this, next_num_leaves, new_pos);
        new_root->m_hash = Read(new_pos);
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
        proof_hashes[i] = Read(proof_positions.first[i]);
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

bool RamForest::Add(const std::vector<Leaf>& leaves)
{
    // Each leaf must have a unique hash, because the leaf position map (m_posmap)
    // can't deal with multiple leaf that have the same hash.
    for (const Leaf& leaf : leaves) {
        auto it = m_posmap.find(leaf.first);
        if (it != m_posmap.end()) {
            // This leaf is already included in the accumulator.
            return false;
        }
    }

    // Preallocate data with the required size.
    ForestState next_state(m_num_leaves + leaves.size());
    for (uint8_t row = 0; row <= next_state.NumRows(); ++row) {
        if (row >= this->m_data.size()) {
            m_data.push_back(std::vector<Hash>());
        }

        m_data.at(row).resize(next_state.m_num_leaves >> row);
    }
    assert(m_data.size() > next_state.NumRows());

    bool ok = Accumulator::Add(leaves);
    assert(next_state.m_num_leaves == m_num_leaves);
    assert(m_posmap.size() == m_num_leaves);

    return ok;
}

bool RamForest::Modify(UndoBatch& undo,
                       const std::vector<Leaf>& leaves,
                       const std::vector<uint64_t>& targets)
{
    if (!RamForest::Remove(targets)) return false;
    if (!BuildUndoBatch(undo, leaves.size(), targets)) return false;
    if (!RamForest::Add(leaves)) return false;

    return true;
}

void RamForest::RestoreRoots()
{
    m_roots.clear();
    std::vector<uint64_t> root_positions = ForestState(m_num_leaves).RootPositions();
    for (const uint64_t& pos : root_positions) {
        m_roots.push_back(Accumulator::MakeNodePtr<RamForest::Node>(this, Read(pos), m_num_leaves, pos));
    }
}

bool RamForest::BuildUndoBatch(UndoBatch& undo, uint64_t num_adds, const std::vector<uint64_t>& targets) const
{
    ForestState prev_state(m_num_leaves + targets.size());

    std::vector<Hash> deleted_hashes;
    for (int i = 0; i < targets.size(); ++i) {
        uint64_t pos = m_num_leaves + static_cast<uint64_t>(i);
        if (m_data.size() == 0 || pos >= m_data[0].size()) return false;
        deleted_hashes.push_back(Read(prev_state, pos));
    }

    undo = UndoBatch(num_adds, targets, deleted_hashes);
    return true;
}

bool RamForest::Undo(const UndoBatch& undo)
{
    if (m_data.size() == 0) return true;

    ForestState prev_state(m_num_leaves + undo.GetDeletedPositions().size() - undo.GetNumAdds());

    auto undo_swaps = prev_state.UndoTransform(undo.GetDeletedPositions());

    // Erase the added leaves from the position map.
    for (uint64_t i = m_num_leaves - undo.GetNumAdds(); i < m_num_leaves; ++i) {
        const Hash& hash = Read(i);
        if (m_posmap.find(hash) == m_posmap.end()) return false;
        m_posmap.erase(hash);
    }

    m_num_leaves -= undo.GetNumAdds();

    // Place all deleted hashes at the end of the bottom row.
    // After this the forest is in the same state as right after the deletion
    // in the previous modification.
    int i = 0;
    std::unordered_set<uint64_t> dirt_set;
    m_data[0].resize(prev_state.m_num_leaves);
    for (const Hash& hash : undo.GetDeletedHashes()) {
        if ((m_num_leaves + i) >= m_data[0].size()) return false;
        m_data[0][m_num_leaves + i] = hash;

        // Check that the hash is not already in the forest.
        if (m_posmap.find(hash) != m_posmap.end()) return false;
        m_posmap[hash] = m_num_leaves + i;
        dirt_set.insert(m_num_leaves + i);
        ++i;
    }

    m_num_leaves = prev_state.m_num_leaves;

    // Swap the delted hashes into their positions pre-deletion.
    for (auto swap_it = undo_swaps.crbegin(); swap_it != undo_swaps.crend(); ++swap_it) {
        auto swap = *swap_it;

        uint64_t range = swap.m_range;
        if (!swap.m_is_range_swap) {
            range = 1;
        }

        for (uint64_t i = 0; i < range; ++i) {
            dirt_set.insert(swap.m_from + i);
            dirt_set.insert(swap.m_to + i);
        }

        SwapRange(swap.m_from, swap.m_to, range);
    }


    // Rehash all the "dirty" parts of the forest.
    std::vector<uint64_t> dirt_list(dirt_set.begin(), dirt_set.end());
    std::sort(dirt_list.begin(), dirt_list.end());

    // Construct the first row of dirt.
    std::vector<NodePtr<RamForest::Node>> dirt;
    for (const uint64_t& pos : dirt_list) {
        uint64_t parent_pos = prev_state.Parent(pos);
        // Skip positions that are past the bottom row root.
        // The parents of those positions do not exist in the new forest.
        if (prev_state.HasRoot(0) && prev_state.RootPosition(0) <= pos) continue;

        // Dont add the same parent to the next row dirt.
        if (dirt.size() != 0 && dirt.back()->m_position == prev_state.Parent(pos)) continue;

        dirt.push_back(Accumulator::MakeNodePtr<RamForest::Node>(this, m_num_leaves, parent_pos));
    }

    for (uint8_t r = 1; r <= prev_state.NumRows(); ++r) {
        m_data[r].resize(m_num_leaves >> r);
        std::vector<NodePtr<RamForest::Node>> next_dirt;

        for (NodePtr<RamForest::Node> dirt_node : dirt) {
            dirt_node->ReHash();
            auto parent = dirt_node->Parent();
            if (parent && (next_dirt.size() == 0 || next_dirt.back()->m_position != parent->m_position)) {
                next_dirt.push_back(std::dynamic_pointer_cast<RamForest::Node>(parent));
            }
        }
        dirt = next_dirt;
    }

    RestoreRoots();

    CHECK_SAFE(m_data[0].size() == m_posmap.size());
    CHECK_SAFE([](const std::unordered_map<Hash, uint64_t, LeafHasher>& posmap,
                  const std::vector<std::vector<Hash>>& data) {
        int pos = 0;
        for (const Hash& hash : data[0]) {
            auto it = posmap.find(hash);
            if (it == posmap.end()) return false;
            if (it->second != pos) return false;
            ++pos;
        }

        return true;
    }(m_posmap, m_data));

    return true;
}

Hash RamForest::GetLeaf(uint64_t pos) const
{
    assert(pos < m_num_leaves);
    return Read(pos);
}

bool RamForest::operator==(const RamForest& other)
{
    std::vector<Hash> roots, other_roots;
    Roots(roots);
    other.Roots(other_roots);
    return m_num_leaves == other.m_num_leaves &&
           roots == other_roots &&
           m_posmap == other.m_posmap;
}

}; // namespace utreexo
