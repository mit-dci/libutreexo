#include "impl.h"
#include <algorithm>
#include <list>

namespace utreexo {

bool AccumulatorImpl::Prove(BatchProof<Hash>& proof, const std::vector<Hash>& target_hashes) const
{
    if (target_hashes.size() == 0) return true;

    std::map<uint64_t, const InternalNode*> proof_nodes;
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
        const InternalNode* sibling{ReadNode(sibing_pos)};
        if (!sibling) return false;
        CHECK_SAFE(sibling->IsDeadEnd());

        proof_nodes.emplace(sibing_pos, sibling);
    }

    std::vector<Hash> proof_hashes;

    // Fetch all nodes contained in the proof by going upwards.
    while (!proof_nodes.empty()) {
        auto node_it{proof_nodes.begin()};
        uint64_t node_pos{node_it->first};
        const InternalNode* node{node_it->second};
        proof_nodes.erase(node_it);

        // Ignore roots.
        if (!node->m_aunt) continue;

        if (proof_nodes.erase(m_current_state.Sibling(node_pos)) == 0) {
            proof_hashes.push_back(node->m_hash);
        }

        // Don't add roots back to proof_nodes.
        if (!node->m_aunt->m_aunt) continue;

        const uint64_t parent_pos{m_current_state.Parent(node_pos)};
        const uint64_t aunt_pos{m_current_state.Sibling(parent_pos)};
        proof_nodes.emplace(aunt_pos, node->m_aunt);
    }

    proof = BatchProof(targets, proof_hashes);
    return true;
}

} // namespace utreexo

