#include "impl.h"

namespace utreexo {

void AccumulatorImpl::AddLeaf(const Hash& hash, const bool remember)
{
    InternalNode* new_root{new InternalNode(hash)};
    if (remember) {
        MarkLeafAsMemorable(new_root);
    }

    for (uint8_t row = 0; row < MAX_TREE_HEIGHT && m_current_state.HasRoot((uint8_t)row); ++row) {
        assert(m_roots.size() > 0);

        InternalNode* left_niece = m_roots.back();
        m_roots.pop_back();
        assert(new_root && left_niece);

        if (left_niece->m_hash == ZOMBIE_ROOT_HASH) {
            delete left_niece;
            continue;
        }

        std::swap(left_niece->m_nieces, new_root->m_nieces);
        SetAuntForNieces(*left_niece);
        SetAuntForNieces(*new_root);

        new_root = new InternalNode(left_niece, new_root);
        SetAuntForNieces(*new_root);
        MaybePruneNieces(*new_root);
    }

    m_roots.push_back(new_root);
    m_current_state = ForestState(m_current_state.m_num_leaves + 1);
}

void AccumulatorImpl::Add(const std::vector<std::pair<Hash, bool>>& new_leaves)
{
    for (auto leaf = new_leaves.cbegin(); leaf < new_leaves.cend(); ++leaf) {
        AddLeaf(/*hash=*/leaf->first, /*remember=*/leaf->second);
    }
}

void AccumulatorImpl::PromoteSibling(InternalNode& parent, InternalNode& aunt, const uint8_t lr_node)
{
    const uint8_t lr_sib{(uint8_t)(lr_node ^ 1)};
    const InternalNode* sibling{aunt.m_nieces[lr_sib]};
    assert(sibling);

    if (HasMemorableMarker(sibling->m_hash)) {
        OverwriteMemorableMarker(sibling->m_hash, &parent);
    }

    // To remove a leaf we promote its sibling to be its parent.
    parent.m_hash = sibling->m_hash;

    InternalNode* node{aunt.m_nieces[lr_node]};
    if (node) {
        // The aunt now points to the children of the sibling.
        aunt.m_nieces[0] = node->m_nieces[0];
        aunt.m_nieces[1] = node->m_nieces[1];

        // The nodes nieces are now owned by the aunt.
        node->m_nieces[0] = nullptr;
        node->m_nieces[1] = nullptr;

        delete node;
    } else {
        aunt.m_nieces[0] = nullptr;
        aunt.m_nieces[1] = nullptr;
    }

    SetAuntForNieces(aunt);
    delete sibling;
}

void AccumulatorImpl::RemoveOne(const InternalNode& sibling, const uint64_t pos)
{
    InternalNode* aunt{sibling.m_aunt};
    if (!aunt) {
        // Mark the root as deleted by replacing its hash with the NULL_HASH.
        uint8_t root_index{m_current_state.RootIndex(pos)};
        m_roots[root_index]->m_hash = ZOMBIE_ROOT_HASH;
        m_roots[root_index]->DeleteNiece(0);
        m_roots[root_index]->DeleteNiece(1);
        return;
    }

    InternalNode* grand_aunt{aunt->m_aunt};
    if (!grand_aunt) {
        // The aunt is a root, so the node we want to delete is its
        // child as well as its niece.
        PromoteSibling(/*parent=*/*aunt, /*aunt=*/*aunt, /*lr_node=*/pos & 1);
        return;
    }

    uint8_t lr_parent{aunt == grand_aunt->m_nieces[0] ? (uint8_t)1 : (uint8_t)0};
    InternalNode* parent{GuaranteeNiece(*grand_aunt, lr_parent)};
    assert(parent);

    PromoteSibling(/*parent=*/*parent, /*aunt=*/*aunt, /*lr_node=*/pos & 1);

    ReHashToTop(*parent, *aunt, m_current_state.Parent(pos));
}

bool AccumulatorImpl::Remove(const std::vector<uint64_t>& targets)
{
    if (targets.size() == 0) return true;

    // Make sure all the leaves we want to remove are cached.
    std::vector<Hash> target_hashes;
    target_hashes.reserve(targets.size());
    for (auto pos = targets.cbegin(); pos < targets.cend(); ++pos) {
        const InternalNode* target_node = ReadNode(*pos);
        if (!target_node) return false;

        auto leaf_it = m_cached_leaves.find(target_node->m_hash);
        if (leaf_it == m_cached_leaves.end()) return false;
        target_hashes.push_back(target_node->m_hash);
    }

    std::vector<uint64_t> deletions = m_current_state.SwaplessTransform(targets);

    uint64_t next_num_leaves{m_current_state.m_num_leaves};
    for (uint64_t deletion : deletions) {
        // Read the sibling of the node to be deleted, unless we are deleting a root.
        uint64_t read_pos{m_current_state.Sibling(deletion)};
        const uint8_t row{m_current_state.DetectRow(deletion)};
        if (m_current_state.HasRoot(row) && deletion == m_current_state.RootPosition(row)) {
            read_pos = deletion;
        }

        const InternalNode* sibling{ReadNode(read_pos)};
        assert(sibling);

        RemoveOne(*sibling, deletion);
    }

    // All leaves that are supposed to be deleted exist in the forest. Now we
    // remove all there memorable markers.
    for (const Hash& target_hash : target_hashes) {
        RemoveMemorableMarkerFromLeaf(target_hash);
    }
    target_hashes.clear();

    // Remove roots that were marked for deletion. RemoveOne marks roots
    // for deletion by replacing them with null pointers in m_roots.
    std::vector<InternalNode*> new_roots;
    for (int i = 0; i < m_roots.size(); ++i) {
        if (m_roots[i]) new_roots.push_back(m_roots[i]);
    }
    m_roots = new_roots;
    m_current_state = ForestState(next_num_leaves);

    return true;
}

bool AccumulatorImpl::Modify(const std::vector<std::pair<Hash, bool>>& new_leaves,
                             const std::vector<uint64_t>& targets)
{
    if (!Remove(targets)) return false;
    Add(new_leaves);
    assert(m_roots.size() == m_current_state.NumRoots());
    return true;
}

} // namespace utreexo
