#ifndef UTREEXO_IMPL_H
#define UTREEXO_IMPL_H

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <string>

#include "accumulator.h"
#include "batchproof.h"
#include "check.h"
#include "crypto/sha512.h"
#include "state.h"

namespace utreexo {

static constexpr uint8_t MAX_TREE_HEIGHT{64}; // sizeof(uint64_t) * 8
static const Hash NULL_HASH{};
// When a root is deleted its hash is set to this value.
static const Hash ZOMBIE_ROOT_HASH{0xDE, 0xAD, 0xBE, 0xEF};

static Hash ParentHash(const Hash& left, const Hash& right)
{
    Hash parent;
    CSHA512 hasher(CSHA512::OUTPUT_SIZE_256);
    hasher.Write(left.data(), 32);
    hasher.Write(right.data(), 32);
    hasher.Finalize256(parent.data());
    return parent;
}

struct InternalNode {
public:
    //! Hash of this node.
    Hash m_hash{NULL_HASH};
    //! Aunt of this node.
    //! NOTE: will be null for roots.
    InternalNode* m_aunt{nullptr};
    //! Nieces of this node (0 is left, 1 is right).
    InternalNode* m_nieces[2]{nullptr, nullptr};

    /** This constructor should only be used when the given nieces are also the children. */
    InternalNode(InternalNode* left_niece, InternalNode* right_niece)
    {
        assert(left_niece && right_niece);
        m_hash = ParentHash(left_niece->m_hash, right_niece->m_hash);
        m_nieces[0] = left_niece;
        m_nieces[1] = right_niece;
    }
    InternalNode(const Hash& hash) : m_hash(hash) {}
    InternalNode() {}
    ~InternalNode()
    {
        DeleteNiece(0);
        DeleteNiece(1);
        m_hash = NULL_HASH;
    }

    void DeleteNiece(const uint8_t lr)
    {
        if (m_nieces[lr] && m_nieces[lr]->m_aunt == this) {
            m_nieces[lr]->m_aunt = nullptr;
            delete m_nieces[lr];
            m_nieces[lr] = nullptr;
        }
    }

    void ReHash(const InternalNode& sibling)
    {
        assert(sibling.m_nieces[0] && sibling.m_nieces[1]);
        m_hash = ParentHash(sibling.m_nieces[0]->m_hash, sibling.m_nieces[1]->m_hash);
    }

    bool IsDeadEnd() const { return !m_nieces[0] && !m_nieces[1]; }
};

struct NodeAndMetadata {
private:
    //! Pointer to the node
    InternalNode* m_node;
    //! The node's position.
    uint64_t m_position;
    //! Whether or not the node has a memorable child.
    bool m_has_memorable_child{false};
    //! Index of the root of which the node is a descendant of.
    uint8_t m_root_index{0};

public:
    NodeAndMetadata(InternalNode& node, const uint64_t pos, const bool has_memorable_child, const uint8_t root_index)
        : m_node(&node), m_position(pos), m_has_memorable_child(has_memorable_child), m_root_index(root_index) {}

    InternalNode& GetNode() const { return *m_node; }
    uint64_t GetPosition() const { return m_position; }
    bool HasMemorableChild() const { return m_has_memorable_child; }
    uint8_t GetRootIndex() const { return m_root_index; }

    NodeAndMetadata& operator=(NodeAndMetadata other)
    {
        std::swap(m_node, other.m_node);
        std::swap(m_position, other.m_position);
        std::swap(m_has_memorable_child, other.m_has_memorable_child);
        std::swap(m_root_index, other.m_root_index);
        return *this;
    }
};

static bool CompareNodeAndMetadataByPosition(const NodeAndMetadata& a, const NodeAndMetadata& b)
{
    return a.GetPosition() < b.GetPosition();
}

class AccumulatorImpl : public Accumulator
{
public:
    ForestState m_current_state{0};

    std::vector<InternalNode*> m_roots;

    std::map<Hash, InternalNode*> m_cached_leaves;

    AccumulatorImpl(const uint64_t num_leaves, const std::vector<Hash>& roots)
    {
        m_current_state = ForestState(num_leaves);
        m_roots.reserve(64);
        for (const Hash& root_hash : roots) {
            m_roots.push_back(new InternalNode(root_hash));
        }
    }
    virtual ~AccumulatorImpl()
    {
        m_cached_leaves.clear();
        while (!m_roots.empty()) {
            InternalNode* root{m_roots.back()};
            m_roots.pop_back();
            delete root;
        }
    }

    InternalNode* WriteNode(uint64_t pos, const Hash& hash, bool allow_overwrite = false)
    {
        // Get the path to the position.
        const auto [tree, path_length, path_bits] = m_current_state.Path(pos);
        assert(tree < m_roots.size());

        InternalNode* sibling{m_roots[tree]};
        assert(sibling);

        if (path_length == 0) {
            if (allow_overwrite) sibling->m_hash = hash;
            if (sibling->m_hash == hash) return sibling;
            return nullptr;
        }

        uint8_t lr_cutoff;
        InternalNode* cutoff{nullptr};

        // Traverse the pollard until the desired position is reached.
        for (uint8_t i = 0; i < path_length - 1; ++i) {
            uint8_t lr = (path_bits >> (path_length - 1 - i)) & 1;

            if (!cutoff && !sibling->m_nieces[lr]) {
                // Remember the highest node that was newly populated so we can
                // easily chop if off if we fail to write the hash.
                cutoff = sibling;
                lr_cutoff = lr;
            }

            sibling = GuaranteeNiece(*sibling, lr);
        }

        InternalNode* node{GuaranteeNiece(*sibling, pos & 1)};
        if (!allow_overwrite && node->m_hash != NULL_HASH && node->m_hash != hash) {
            // A hash already exists here and we are trying to replace it with
            // a different one. This should not be allowed.
            if (cutoff) cutoff->DeleteNiece(lr_cutoff);
            return nullptr;
        }

        node->m_hash = hash;

        return node;
    }
    InternalNode* ReadNode(uint64_t pos) const
    {
        // TODO add check that the position can exist in m_current_state.

        // Get the path to the position.
        const auto [tree, path_length, path_bits] = m_current_state.Path(pos);
        assert(tree < m_roots.size());

        InternalNode* node{m_roots[tree]};
        const InternalNode* sibling{node};

        // Traverse the pollard until the desired position is reached.
        for (uint8_t i = 0; i < path_length; ++i) {
            uint8_t lr = (path_bits >> (path_length - 1 - i)) & 1;
            uint8_t lr_sib = m_current_state.Sibling(lr);

            if (!sibling) {
                return nullptr;
            }

            node = sibling->m_nieces[lr_sib];
            sibling = sibling->m_nieces[lr];
        }

        return node;
    }
    /**
     * Read a hash from the forest given its position.
     *
     * Return the hash or std::nullopt if the position is not cached.
     */
    std::optional<const Hash> Read(uint64_t pos) const
    {
        InternalNode* node{ReadNode(pos)};
        if (node) {
            return std::optional<const Hash>{node->m_hash};
        }

        return std::nullopt;
    }

    /** Calcaulate a node's position by going up using the aunt pointers. */
    uint64_t ComputePosition(const InternalNode& node) const
    {
        uint64_t path{0};
        uint8_t path_length{0};

        const InternalNode* current{&node};
        while (path_length < MAX_TREE_HEIGHT) {
            const InternalNode* aunt{current->m_aunt};
            if (!aunt) break;

            assert(aunt->m_nieces[0] == current || aunt->m_nieces[1] == current);
            const uint8_t lr{aunt->m_nieces[0] == current ? (uint8_t)0 : (uint8_t)1};

            path <<= 1;
            path |= path_length == 0 ? lr : lr ^ 1;

            current = aunt;
            ++path_length;
        }

        auto root_it = std::find_if(m_roots.cbegin(), m_roots.cend(), [&current](const InternalNode* root) {
            return root == current;
        });
        assert(root_it != m_roots.cend());

        int root_index = std::distance(m_roots.cbegin(), root_it);
        CHECK_SAFE(root_index < m_current_state.RootPositions().size());

        uint64_t position{m_current_state.RootPositions()[root_index]};
        for (int i = 0; i < path_length; ++i) {
            position = m_current_state.Child(position, path & 1);

            path >>= 1;
        }

        return position;
    }

    /** Mark a leaf as memorable by storing it in m_cached_leaves. */
    void MarkLeafAsMemorable(InternalNode* node)
    {
        assert(node);
        m_cached_leaves.emplace(node->m_hash, node);
    }
    /**
     * Remove the memorable marker from a leaf by erasing it from
     * m_cached_leaves and pointing its left niece to nullptr.
     */
    void RemoveMemorableMarkerFromLeaf(const Hash& hash)
    {
        m_cached_leaves.erase(hash);
    }
    /** Check whether a hash is marked as memorable. */
    bool HasMemorableMarker(const Hash& hash) const
    {
        // TODO this is gonna cause a lot of map look ups, can we find a better
        // way for storing the memorable marker.
        auto it = m_cached_leaves.find(hash);
        return it != m_cached_leaves.end();
    }
    void OverwriteMemorableMarker(const Hash& hash, InternalNode* new_node)
    {
        m_cached_leaves[hash] = new_node;
    }
    /**
     * Attempt to prune the nieces of a node.
     *
     * Each niece is considered for pruning individually and is only pruned if
     * it is a dead end and neither it self nor its sibling is marked as
     * memorable.
     */
    void MaybePruneNieces(InternalNode& node)
    {
        bool left_niece_memorable{node.m_nieces[0] && HasMemorableMarker(node.m_nieces[0]->m_hash)};
        bool right_niece_memorable{node.m_nieces[1] && HasMemorableMarker(node.m_nieces[1]->m_hash)};

        if (node.m_nieces[0] && node.m_nieces[0]->IsDeadEnd() &&
            !left_niece_memorable && !right_niece_memorable) node.DeleteNiece(0);
        if (node.m_nieces[1] && node.m_nieces[1]->IsDeadEnd() &&
            !right_niece_memorable && !left_niece_memorable) node.DeleteNiece(1);
    }
    /** Go up using the aunt pointers and prune. */
    void PruneBranch(InternalNode& leaf)
    {
        InternalNode* aunt{leaf.m_aunt};
        if (!aunt) {
            // The leaf is a root and there is nothing else todo besides
            // removing the memorable marker.
            return;
        }
        const uint8_t lr_sib{aunt->m_nieces[0] == &leaf ? (uint8_t)1 : (uint8_t)0};
        InternalNode* sibling{aunt->m_nieces[lr_sib]};
        if (sibling && HasMemorableMarker(sibling->m_hash)) {
            // Do not prune the proof if the sibling is still marked as
            // memorable.
            return;
        }

        while (aunt) {
            MaybePruneNieces(*aunt);
            aunt = aunt->m_aunt;
        }
    }
    /** Set the aunt pointer for the nieces of a node. */
    void SetAuntForNieces(InternalNode& aunt)
    {
        if (aunt.m_nieces[0]) aunt.m_nieces[0]->m_aunt = &aunt;
        if (aunt.m_nieces[1]) aunt.m_nieces[1]->m_aunt = &aunt;
    }

    /**
     * Guarantee that the `lr_niece` (0 = left, 1 = right) niece of the aunt
     * exists. A new niece is created should it not currently be cached.
     *
     * NOTE: nieces created through this do not have any nieces themselves and
     * will therefore be pruned shortly after being created in `Verify` or
     * `Remove`.
     *
     * Return the niece (guaranted to be non-null);
     */
    InternalNode* GuaranteeNiece(InternalNode& aunt, const uint8_t lr_niece)
    {
        if (!aunt.m_nieces[lr_niece]) {
            // This will be pruned away since we dont set its nieces.
            // TODO can we do without the extra allocation?
            aunt.m_nieces[lr_niece] = new InternalNode(NULL_HASH);
            aunt.m_nieces[lr_niece]->m_aunt = &aunt;
        }

        return aunt.m_nieces[lr_niece];
    }
    /**
     * Guarantee that the parent of a node exists. The parent will be created
     * wiht the NULL_HASH if it does not exist.
     *
     * NOTE: parents created through this do not have any nieces themselves and
     * will therefore be pruned shortly after being created in `Verify` or
     * `Remove`.
     *
     * Return the parent (guaranted to be non-null);
     */
    InternalNode* GuaranteeParent(const InternalNode& node, uint64_t parent_pos)
    {
        InternalNode* aunt{node.m_aunt};
        // Roots do not have parents
        if (!aunt) return nullptr;

        InternalNode* grand_aunt{aunt->m_aunt};
        // Aunt is a root
        if (!grand_aunt) return aunt;

        return GuaranteeNiece(*grand_aunt, parent_pos & 1);
    }


    /** Rehash and prune a branch until a root is reached. */
    void ReHashToTop(InternalNode& node, InternalNode& sibling, const uint64_t node_pos)
    {
        if (sibling.m_nieces[0] && sibling.m_nieces[1]) {
            node.ReHash(sibling);
            MaybePruneNieces(sibling);
        }

        InternalNode* aunt{node.m_aunt};
        if (!aunt) {
            // node is a root
            assert(m_current_state.HasRoot(m_current_state.DetectRow(node_pos)));
            assert(m_current_state.RootPosition(m_current_state.DetectRow(node_pos)) == node_pos);
            return;
        }

        const uint64_t parent_pos{m_current_state.Parent(node_pos)};
        InternalNode* grand_aunt{aunt->m_aunt};
        if (!grand_aunt) {
            // aunt is a root.
            ReHashToTop(*aunt, *aunt, parent_pos);
            return;
        }

        uint8_t lr_parent{(uint8_t)(parent_pos & 1)};
        InternalNode* parent{GuaranteeNiece(*grand_aunt, lr_parent)};
        ReHashToTop(*parent, *aunt, parent_pos);
    }

    // modify.cpp

    /**
     * Add a single leaf to the accumulator given its hash and whether or not
     * it should be cached.
     *
     * Should the leaf be added as memorable, it is possible to prove its
     * existence using `Prove` as well as remove it using `Remove`. A leaf can
     * be uncached/forgotten (but not removed from the accumulator) using
     * `Uncache`.
     *
     * Adding a leaf can not fail and will always succeed. However the caller
     * should make sure that no duplicate leaves are added as the caching
     * can only deal with unique leaves.
     */
    void AddLeaf(const Hash& hash, const bool remember);
    /** Add multiple leaves to the accumulator by adding each leaf individually. */
    void Add(const std::vector<std::pair<Hash, bool>>& new_leaves);

    /**
     * Promotes the sibling of the aunt's `lr_node` (0 = left, 1 = right) niece
     * to the parent, effectively removing the niece.
     */
    void PromoteSibling(InternalNode& parent, InternalNode& aunt, const uint8_t lr_node);
    /**
     * Remove a single leaf/node by promoting its sibling to be its parent.
     *
     * Return whether or not the node was a root.
     */
    void RemoveOne(const InternalNode& sibling, const uint64_t pos);
    /**
     * Remove multiple leaves given their positions.
     *
     * Removing leaves will fail if one or more of the leaves are not cached.
     * To remove a leaf its inclusion proof has to be cached with in the
     * accumulator or the accumulator could not be updated to exclude the leaf.
     *
     * Return whether or not removing *all* leaves succeeded.
     */
    bool Remove(const std::vector<uint64_t>& targets);

    // verify.cpp

    bool IngestProof(std::map<uint64_t, InternalNode*>& verification_map, const BatchProof<Hash>& proof, const std::vector<Hash>& target_hashes);

    /** Implement `Accumulator` interface */

    bool Verify(const BatchProof<Hash>& proof, const std::vector<Hash>& target_hashes) override;
    bool Modify(const std::vector<std::pair<Hash, bool>>& new_leaves, const std::vector<uint64_t>& targets) override;
    // undo.cpp
    void UndoOne(InternalNode& parent, InternalNode& aunt, const uint8_t lr_sib);
    bool Undo(const uint64_t previous_num_leaves,
              const Hashes& previous_root_hashes,
              const BatchProof<Hash>& previous_proof,
              const Hashes& previous_targets) override;
    bool Prove(BatchProof<Hash>& proof, const std::vector<Hash>& target_hashes) const override;
    void Uncache(const Hash& leaf_hash) override
    {
        auto leaf_it = m_cached_leaves.find(leaf_hash);

        // If this leaf isn't cached, we can quit early.
        if (leaf_it == m_cached_leaves.end()) return;

        InternalNode* leaf{leaf_it->second};

        // Remove the memorable marker so that we can prune up to the roots.
        RemoveMemorableMarkerFromLeaf(leaf->m_hash);

        PruneBranch(*leaf);
    }

    bool IsCached(const Hash& leaf_hash) const override
    {
        return HasMemorableMarker(leaf_hash);
    }

    Hashes GetCachedLeaves() const override
    {
        std::vector<NodeAndMetadata> leaves_and_positions;
        for (const auto& [hash, node] : m_cached_leaves) {
            leaves_and_positions.emplace_back(*node, ComputePosition(*node), false, 0);
        }

        std::sort(leaves_and_positions.begin(), leaves_and_positions.end(), CompareNodeAndMetadataByPosition);

        std::vector<Hash> leaf_hashes;
        for (const NodeAndMetadata& leaf_and_pos : leaves_and_positions) {
            leaf_hashes.push_back(leaf_and_pos.GetNode().m_hash);
        }

        return leaf_hashes;
    }

    std::tuple<uint64_t, std::vector<Hash>> GetState() const override
    {
        std::vector<Hash> root_hashes(m_roots.size());
        for (int i = 0; i < m_roots.size(); ++i) {
            root_hashes[i] = m_roots[i]->m_hash;
        }
        return {m_current_state.m_num_leaves, root_hashes};
    }
};

} // namespace utreexo

#endif
