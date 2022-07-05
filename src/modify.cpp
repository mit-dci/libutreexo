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

        // The nodes nieces (i.e. the siblings children) are now owned by the aunt.
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
        // Mark the root as deleted by replacing its hash with the ZOMBIE_ROOT_HASH.
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

    // Hashes of the leaves that we want to remove.
    std::vector<Hash> target_hashes;
    target_hashes.reserve(targets.size());

    // Siblings of the leaves that we want to remove.
    std::map<uint64_t, InternalNode*> target_siblings;
    for (auto pos = targets.cbegin(); pos < targets.cend(); ++pos) {
        // The leaves we want to remove (targets) have to be cached.
        InternalNode* target_node{ReadNode(*pos)};
        if (!target_node) return false;

        auto leaf_it = m_cached_leaves.find(target_node->m_hash);
        if (leaf_it == m_cached_leaves.end()) return false;
        target_hashes.push_back(target_node->m_hash);

        // The siblings of the leaves we want to remove have to be cached.
        InternalNode* sibling{target_node->m_aunt ? ReadNode(m_current_state.Sibling(*pos)) : target_node};
        if (!sibling) return false;

        uint64_t sibling_pos{target_node->m_aunt ? m_current_state.Sibling(*pos) : *pos};
        target_siblings.emplace(sibling_pos, sibling);
    }

    while (!target_siblings.empty()) {
        auto it{target_siblings.begin()};
        const uint64_t pos{it->first};
        InternalNode* sibling{it->second};
        target_siblings.erase(it);

        if (sibling->m_aunt) {
            // If we are not removing a root, then we check if we are removing
            // two siblings.

            auto node_it{target_siblings.find(m_current_state.Sibling(pos))};
            if (node_it != target_siblings.end()) {
                // If we are removing siblings then we can just remove the parent,
                // so we add the aunt to `target_siblings`.
                const uint64_t parent_pos{m_current_state.Parent(pos)};
                const uint64_t aunt_pos{sibling->m_aunt->m_aunt ? m_current_state.Sibling(parent_pos) : parent_pos};
                target_siblings.emplace(aunt_pos, sibling->m_aunt);

                target_siblings.erase(node_it);
                continue;
            }

            RemoveOne(*sibling, m_current_state.Sibling(pos));
        } else {
            RemoveOne(*sibling, pos);
        }
    }

    // All leaves that are supposed to be deleted exist in the forest. Now we
    // remove all there memorable markers.
    for (const Hash& target_hash : target_hashes) {
        RemoveMemorableMarkerFromLeaf(target_hash);
    }

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
