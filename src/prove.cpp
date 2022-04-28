#include "impl.h"
#include <algorithm>
#include <list>

namespace utreexo {

bool AccumulatorImpl::Prove(BatchProof<Hash>& proof, const std::vector<Hash>& target_hashes) const
{
    if (target_hashes.size() == 0) return true;

    std::list<NodeAndMetadata> siblings;
    std::vector<uint64_t> targets;

    // Get all the siblings of the targets.
    for (auto hash = target_hashes.cbegin(); hash < target_hashes.cend(); ++hash) {
        auto leaf_it = m_cached_leaves.find(*hash);
        if (leaf_it == m_cached_leaves.end()) return false;

        InternalNode* leaf_node{leaf_it->second};
        const uint64_t leaf_pos{ComputePosition(*leaf_node)};
        targets.push_back(leaf_pos);

        if (!leaf_node->m_aunt) {
            // Roots have no proof.
            continue;
        }

        const uint64_t sibing_pos{m_current_state.Sibling(leaf_pos)};
        InternalNode* sibling{ReadNode(sibing_pos)};
        if (!sibling) return false;
        CHECK_SAFE(sibling->IsDeadEnd());

        siblings.emplace_back(*sibling, sibing_pos, false, m_current_state.RootIndex(sibing_pos));
    }

    uint8_t row{siblings.empty() ? (uint8_t)0 : m_current_state.DetectRow(siblings.front().GetPosition())};
    auto row_change = [state = m_current_state, &row](const NodeAndMetadata& node_and_pos) {
        return state.DetectRow(node_and_pos.GetPosition()) > row;
    };

    // Fetch all nodes contained in the proof by going upwards.
    std::list<NodeAndMetadata> proof_nodes;
    while (!siblings.empty()) {
        siblings.sort(CompareNodeAndMetadataByPosition);

        auto row_start = siblings.begin();
        auto row_end = std::find_if(siblings.begin(), siblings.end(), row_change);

        int next_row{row_end == siblings.end() ? (int)row : (int)m_current_state.DetectRow(row_end->GetPosition())};
        while (row_start != row_end) {
            bool more_than_one_left{std::next(row_start) != row_end && std::next(row_start, 2) != row_end};
            if (more_than_one_left && // Maybe two siblings?
                row_start->GetPosition() == m_current_state.Sibling(std::next(row_start, 1)->GetPosition())) {
                ++row_start;
                siblings.pop_front();
            } else {
                proof_nodes.push_back(*row_start);
            }

            if (row_start->GetNode().m_aunt && row_start->GetNode().m_aunt->m_aunt) {
                const uint64_t aunt_pos{m_current_state.Sibling(m_current_state.Parent(row_start->GetPosition()))};
                siblings.push_back(NodeAndMetadata{*row_start->GetNode().m_aunt, aunt_pos, false, row_start->GetRootIndex()});
                next_row = (int)row + 1;
            }

            ++row_start;
            siblings.pop_front();
        }

        if (next_row >= MAX_TREE_HEIGHT) return false;
        row = (uint8_t)next_row;
    }

    // TODO is this needed?
    proof_nodes.sort(CompareNodeAndMetadataByPosition);

    // Convert the proof nodes to hashes.
    std::vector<Hash> proof_hashes;
    proof_hashes.reserve(proof_nodes.size());
    while (!proof_nodes.empty()) {
        proof_hashes.push_back(proof_nodes.front().GetNode().m_hash);
        proof_nodes.pop_front();
    }

    proof = BatchProof(targets, proof_hashes);
    return true;
}

} // namespace utreexo

