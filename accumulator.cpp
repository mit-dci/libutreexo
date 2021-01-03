#include <accumulator.h>
#include <check.h>
#include <crypto/sha256.h>
#include <iostream>
#include <stdio.h>
#include <uint256.h>

void Accumulator::Modify(const std::vector<std::shared_ptr<Accumulator::Leaf>>& leaves, const std::vector<uint64_t>& targets)
{
    this->Remove(targets);
    this->Add(leaves);
}

const std::vector<uint256> Accumulator::Roots() const
{
    std::vector<uint256> result;
    result.reserve(m_roots.size());

    for (auto root : m_roots) {
        result.push_back(root->Hash());
    }

    return result;
}

uint256 Accumulator::ParentHash(const uint256& left, const uint256& right)
{
    CHECK_SAFE(!left.IsNull());
    CHECK_SAFE(!right.IsNull());

    CSHA256 hasher;

    // copy the two hashes into one 64 byte buffer
    uint8_t data[64];
    memcpy(data, left.begin(), 32);
    memcpy(data + 32, right.begin(), 32);
    hasher.Write(data, 64);

    // finalize the hash and write it into parentHash
    uint256 parent_hash;
    hasher.Finalize(parent_hash.begin());

    return parent_hash;
}

void Accumulator::PrintRoots(const std::vector<NodePtr<Accumulator::Node>>& roots) const
{
    for (auto root : roots) {
        std::cout << "root: " << root->m_position << ":" << root->Hash().GetHex() << std::endl;
    }
}

void Accumulator::Add(const std::vector<std::shared_ptr<Accumulator::Leaf>>& leaves)
{
    // Adding leaves can't be batched, so we add one by one.
    for (auto leaf = leaves.begin(); leaf < leaves.end(); ++leaf) {
        int root = m_roots.size() - 1;
        // Create a new leaf and append it to the end of roots.
        uint256 leaf_hash = (*leaf)->Hash();
        NodePtr<Accumulator::Node> new_root = this->NewLeaf(leaf_hash);

        // Merge the last two roots into one for every consecutive root from row 0 upwards.
        for (uint8_t row = 0; m_state.HasRoot(row); ++row) {
            new_root = this->MergeRoot(
                m_state.Parent(new_root->m_position),
                Accumulator::ParentHash(m_roots[root]->Hash(), new_root->Hash()));
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

void Accumulator::Remove(const std::vector<uint64_t>& targets)
{
    if (targets.size() == 0) {
        return;
    }

    if (m_state.m_num_leaves < targets.size()) {
        // TODO: error deleting more targets than elemnts in the accumulator.
        return;
    }

    if (!IsSortedNoDupes(targets)) {
        // TODO: error targets are not sorted or contain duplicates.
        return;
    }

    if (targets.back() >= m_state.m_num_leaves) {
        // TODO: error targets not in the accumulator.
        return;
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
                NodePtr<Accumulator::Node> swap_dirt = this->SwapSubTrees(swap.m_from, swap.m_to);
                dirty_nodes.push_back(swap_dirt);
            }
        }

        // Rehash all the dirt after swapping.
        for (NodePtr<Accumulator::Node> dirt : dirty_nodes) {
            dirt->ReHash();
            NodePtr<Accumulator::Node> parent = dirt->Parent();
            if (parent && (next_dirty_nodes.size() == 0 || next_dirty_nodes.back()->m_position != parent->m_position)) {
                next_dirty_nodes.push_back(parent);
            }
        }

        dirty_nodes = next_dirty_nodes;
    }

    ForestState next_state(m_state.m_num_leaves - targets.size());
    this->FinalizeRemove(next_state);
    m_state.Remove(targets.size());
}

// Accumulator::BatchProof
bool Accumulator::BatchProof::Verify(ForestState state, const std::vector<uint256> roots, const std::vector<uint256> target_hashes) const
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
    std::vector<std::pair<uint64_t, uint256>> target_nodes;
    target_nodes.reserve(targets.size() * state.NumRows());
    for (uint64_t i = 0; i < this->targets.size(); ++i) {
        target_nodes.push_back(std::make_pair(this->targets[i], target_hashes[i]));
    }

    // root_candidates holds the roots that were computed and have to be
    // compared to the actual roots at the end.
    std::vector<uint256> root_candidates;
    root_candidates.reserve(roots.size());

    // Handle the row 0 root.
    if (state.HasRoot(0) && this->targets.back() == state.RootPosition(0)) {
        root_candidates.push_back(target_nodes.back().second);
        target_nodes.pop_back();
    }

    uint64_t proof_index = 0;
    for (uint64_t target_index = 0; target_index < target_nodes.size();) {
        std::pair<uint64_t, uint256> target = target_nodes[target_index], proof;

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
        uint256 parent_hash = Accumulator::ParentHash(left.second, right.second);

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
    for (uint256 root : roots) {
        if (root_candidates.size() > rootMatches && root.Compare(root_candidates[rootMatches]) == 0) {
            ++rootMatches;
        }
    }

    if (rootMatches != root_candidates.size()) {
        // TODO: error not all roots matched.
        return false;
    }

    return true;
}

void Accumulator::BatchProof::Print()
{
    std::cout << "targets: ";
    print_vector(this->targets);

    std::cout << "proof: ";
    for (int i = 0; i < this->proof.size(); ++i) {
        std::cout << proof[i].GetHex().substr(60, 64) << ", ";
    }

    std::cout << std::endl;
}
