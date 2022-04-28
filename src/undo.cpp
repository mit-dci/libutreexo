#include "impl.h"

namespace utreexo {

void AccumulatorImpl::UndoOne(InternalNode& parent, InternalNode& aunt, const uint8_t lr_sib)
{
    InternalNode* node{new InternalNode(NULL_HASH)};
    node->m_nieces[0] = aunt.m_nieces[0];
    node->m_nieces[1] = aunt.m_nieces[1];
    SetAuntForNieces(*node);

    aunt.m_nieces[lr_sib] = new InternalNode(parent.m_hash);
    aunt.m_nieces[lr_sib ^ 1] = node;
    SetAuntForNieces(aunt);

    if (HasMemorableMarker(parent.m_hash)) {
        OverwriteMemorableMarker(parent.m_hash, aunt.m_nieces[lr_sib]);
    }
}

bool AccumulatorImpl::Undo(const uint64_t previous_num_leaves,
                           const Hashes& previous_root_hashes,
                           const BatchProof<Hash>& previous_proof,
                           const Hashes& previous_targets)
{
    ForestState previous_state(previous_num_leaves);
    const std::vector<uint64_t> previous_root_positions{m_current_state.RootPositions(previous_num_leaves)};

    if (previous_root_positions.size() != previous_root_hashes.size()) return false;

    // Forget all leaves that were added in the previous modification.
    for (uint64_t pos = previous_num_leaves; pos < m_current_state.m_num_leaves; ++pos) {
        InternalNode* node{ReadNode(pos)};
        if (!node) continue;
        RemoveMemorableMarkerFromLeaf(node->m_hash);
    }

    // Create new roots and retain the current cache.
    std::vector<InternalNode*> previous_roots;
    std::vector<InternalNode*> siblings;
    auto prev_root_pos{previous_root_positions.cbegin()};
    for (const Hash& prev_root_hash : previous_root_hashes) {
        const uint8_t row{m_current_state.DetectRow(*prev_root_pos)};
        const bool is_current_root{m_current_state.HasRoot(row) && m_current_state.RootPosition(row) == *prev_root_pos};

        previous_roots.push_back(new InternalNode(prev_root_hash));
        InternalNode* prev_root{ReadNode(*prev_root_pos)};
        InternalNode* sibling{is_current_root ? prev_root : ReadNode((*prev_root_pos) ^ 1)};
        siblings.push_back(sibling);

        if (prev_root && HasMemorableMarker(prev_root->m_hash)) {
            previous_roots.back()->m_hash = prev_root->m_hash;
            OverwriteMemorableMarker(prev_root->m_hash, previous_roots.back());
        }

        ++prev_root_pos;
    }

    for (int i = 0; i < siblings.size(); ++i) {
        if (siblings[i]) {
            std::swap(previous_roots[i]->m_nieces, siblings[i]->m_nieces);
            SetAuntForNieces(*siblings[i]);
        }

        SetAuntForNieces(*previous_roots[i]);
    }

    // Delete current roots.
    while (!m_roots.empty()) {
        InternalNode* root{m_roots.back()};
        delete root;
        m_roots.pop_back();
    }

    // Set new state.
    m_roots = previous_roots;
    m_current_state = previous_state;

    // Undo previous deletion moves to preserve the cache.
    std::vector<uint64_t> previous_deletions{previous_state.SwaplessTransform(previous_proof.GetSortedTargets())};
    std::reverse(previous_deletions.begin(), previous_deletions.end());

    for (uint64_t del_pos : previous_deletions) {
        const uint8_t del_row{m_current_state.DetectRow(del_pos)};
        const bool del_is_root{m_current_state.HasRoot(del_row) && m_current_state.RootPosition(del_row) == del_pos};
        if (del_is_root) continue;

        const uint64_t parent_pos{m_current_state.Parent(del_pos)};
        const bool parent_is_root{m_current_state.HasRoot(del_row + 1) && m_current_state.RootPosition(del_row + 1) == parent_pos};
        const uint64_t aunt_pos{parent_is_root ? parent_pos : m_current_state.Sibling(parent_pos)};

        InternalNode* del_parent{ReadNode(parent_pos)};
        if (!del_parent) {
            del_parent = WriteNode(parent_pos, NULL_HASH, /*allow_overwrite=*/true);
        }

        InternalNode* del_aunt{ReadNode(aunt_pos)};
        if (!del_aunt) {
            del_aunt = WriteNode(aunt_pos, NULL_HASH, /*allow_overwrite=*/true);
        }

        UndoOne(*del_parent, *del_aunt, (del_pos & 1) ^ 1);
    }

    // Write target hashes and store target nodes for rehashing later.
    std::vector<NodeAndMetadata> target_nodes;
    for (int i = 0; i < previous_proof.GetTargets().size(); ++i) {
        const uint64_t pos{previous_proof.GetSortedTargets()[i]};
        const Hash& hash{previous_targets.at(i)};
        InternalNode* node{WriteNode(pos, hash, /*allow_overwrite=*/true)};
        target_nodes.push_back(NodeAndMetadata{*node, pos, false, 0});

        // All restored targets are marked as memorable.
        MarkLeafAsMemorable(node);
    }

    std::vector<uint64_t> proof_positions = m_current_state.SimpleProofPositions(previous_proof.GetSortedTargets());
    for (int i = 0; i < proof_positions.size(); ++i) {
        const uint64_t pos{proof_positions[i]};
        const Hash& hash{previous_proof.GetHashes().at(i)};

        WriteNode(pos, hash, /*allow_overwrite=*/true);
    }

    // ReHash

    for (NodeAndMetadata& node_and_meta : target_nodes) {
        InternalNode* aunt{node_and_meta.GetNode().m_aunt};
        if (!aunt) continue;

        const uint8_t lr_sib{aunt->m_nieces[0] == &node_and_meta.GetNode() ? (uint8_t)1 : (uint8_t)0};
        InternalNode* sibling{aunt->m_nieces[lr_sib]};
        // Already pruned
        if (!sibling) continue;

        ReHashToTop(node_and_meta.GetNode(), *sibling, node_and_meta.GetPosition());
    }

    return true;
}

} // namespace utreexo
