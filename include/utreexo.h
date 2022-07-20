#ifndef UTREEXO_UTREEXO_H
#define UTREEXO_UTREEXO_H

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <stack>
#include <stdexcept>
#include <string>

#include "crypto/common.h"
#include "crypto/sha512.h"

namespace utreexo {

template <typename Hash>
class BatchProof;

template <typename Hash>
class Accumulator
{
public:
    static_assert(sizeof(Hash) == 32, "hash type has to be exactly 32 byte in size");

    using Leaves = std::vector<std::pair<Hash, bool>>;
    using Targets = std::vector<uint64_t>;
    using Hashes = std::vector<Hash>;

    virtual ~Accumulator() {}

    /**
     * Verify the existence of multiple leaves, given their hashes
     * (`target_hashes`) and an inclusion `proof`.
     *
     * `target_hashes` needs to be in ascending order according to the leaf
     * positions.
     */
    [[nodiscard]] virtual bool Verify(const BatchProof<Hash>& proof,
                                      const Hashes& target_hashes) = 0;

    /**
     * Modify the accumulator by adding new leaves and deleting targets.
     *
     * The positions specified for deletion (`targets`) have to be cached in
     * the accumulator for the deletion to succeed. Leaves will be cached if
     * they were added as memorable during a modification or if they were
     * ingested during verification. The positions also need to be sorted in
     * ascending order.
     *
     * Return whether or not modifying the accumulator succeeded.
     */
    [[nodiscard]] virtual bool Modify(const Leaves& new_leaves,
                                      const Targets& targets) = 0;

    /** Undo a previous modification to the accumulator. */
    [[nodiscard]] virtual bool Undo(const uint64_t previous_num_leaves,
                                    const Hashes& previous_roots,
                                    const BatchProof<Hash>& previous_proof,
                                    const Hashes& previous_targets) = 0;

    /**
     * Prove the existence of a set of targets (leaf hashes).
     *
     * Only the existence of cached leaves can be proven. Leaves will be cached
     * if they were added as memorable during a modification or if they were
     * ingested during verification.
     *
     * Return whether or not the proof could be created.
     */
    [[nodiscard]] virtual bool Prove(BatchProof<Hash>& proof,
                                     const Hashes& target_hashes) const = 0;

    /**
     * Uncache a leaf from the accumulator. This can not fail as the leaf will
     * either be forgotton or the leaf did not exist in the first place in
     * which case there is nothing todo.
     */
    virtual void Uncache(const Hash& leaf_hash) = 0;

    /** Check whether or not a leaf is cached in the accumulator. */
    virtual bool IsCached(const Hash& leaf_hash) const = 0;

    /** Return all cached leaf hashes. */
    virtual Hashes GetCachedLeaves() const = 0;

    /** Return the state (root bits, merkle forest roots) of the accumulator. */
    virtual std::tuple<uint64_t, Hashes> GetState() const = 0;

    virtual void Serialize(std::vector<uint8_t>& result) const = 0;
    virtual void Unserialize(const std::vector<uint8_t>& bytes) = 0;
};

template <typename Hash>
inline static std::unique_ptr<Accumulator<Hash>> Make(const uint64_t num_leaves, const std::vector<Hash>& roots);
template <typename Hash>
inline static std::unique_ptr<Accumulator<Hash>> MakeEmpty();

/** BatchProof represents a proof for multiple leaves. */
template <typename Hash>
class BatchProof
{
private:
    //! The unsorted/sorted lists of leaf positions that are being proven.
    std::vector<uint64_t> m_targets, m_sorted_targets;

    //! The proof hashes for the targets.
    std::vector<Hash> m_proof;

public:
    BatchProof(const std::vector<uint64_t>& targets, std::vector<Hash> proof)
        : m_targets(targets), m_sorted_targets(targets), m_proof(proof)
    {
        std::sort(m_sorted_targets.begin(), m_sorted_targets.end());
    }

    BatchProof() {}

    const std::vector<uint64_t>& GetTargets() const { return m_targets; }
    const std::vector<uint64_t>& GetSortedTargets() const { return m_sorted_targets; }
    const std::vector<Hash>& GetHashes() const { return m_proof; }

    bool operator==(const BatchProof& other)
    {
        return m_targets.size() == other.m_targets.size() &&
               m_proof.size() == other.m_proof.size() &&
               m_targets == other.m_targets && m_proof == other.m_proof;
    }
};

namespace detail {

static constexpr uint8_t MAX_TREE_HEIGHT{64}; // sizeof(uint64_t) * 8
static const std::array<uint8_t, 32> NULL_HASH{};
// When a root is deleted its hash is set to this value.
static const std::array<uint8_t, 32> ZOMBIE_ROOT_HASH{0xDE, 0xAD, 0xBE, 0xEF};

template <typename Hash>
static Hash ParentHash(const Hash& left, const Hash& right)
{
    Hash parent;
    CSHA512 hasher(CSHA512::OUTPUT_SIZE_256);
    hasher.Write(left.data(), 32);
    hasher.Write(right.data(), 32);
    hasher.Finalize256(parent.data());
    return parent;
}

template <typename Hash>
inline static uint64_t GetUint64(const Hash& h, int pos)
{
    const uint8_t* ptr = h.data() + pos * 8;
    return ((uint64_t)ptr[0]) |
           ((uint64_t)ptr[1]) << 8 |
           ((uint64_t)ptr[2]) << 16 |
           ((uint64_t)ptr[3]) << 24 |
           ((uint64_t)ptr[4]) << 32 |
           ((uint64_t)ptr[5]) << 40 |
           ((uint64_t)ptr[6]) << 48 |
           ((uint64_t)ptr[7]) << 56;
}
// Compare hashes of different type.
// TODO is this slower than memcmp?
template <typename H1, typename H2>
inline static bool CompareHashes(const H1& a, const H2& b)
{
    static_assert(sizeof(a) == sizeof(b), "hash types need to be of the same size");
    static_assert(sizeof(a) == 32, "hash types need to be 32 bytes in size");

    return GetUint64(a, 0) == GetUint64(b, 0) &&
           GetUint64(a, 1) == GetUint64(b, 1) &&
           GetUint64(a, 2) == GetUint64(b, 2) &&
           GetUint64(a, 3) == GetUint64(b, 3);
}

template <typename Hash>
struct InternalNode {
public:
    //! Hash of this node.
    Hash m_hash;
    //! Aunt of this node.
    //! NOTE: will be null for roots.
    InternalNode<Hash>* m_aunt{nullptr};
    //! Nieces of this node (0 is left, 1 is right).
    InternalNode<Hash>* m_nieces[2]{nullptr, nullptr};

    /** This constructor should only be used when the given nieces are also the children. */
    InternalNode<Hash>(InternalNode<Hash>* left_niece, InternalNode<Hash>* right_niece)
    {
        assert(left_niece && right_niece);
        m_hash = ParentHash(left_niece->m_hash, right_niece->m_hash);
        m_nieces[0] = left_niece;
        m_nieces[1] = right_niece;
    }
    template <typename H>
    InternalNode<Hash>(const H& hash)
    {
        SetHash(hash);
    }
    InternalNode<Hash>()
    {
        SetHash(NULL_HASH);
    }
    ~InternalNode<Hash>()
    {
        DeleteNiece(0);
        DeleteNiece(1);
    }

    template <typename H>
    void SetHash(const H& hash)
    {
        std::memcpy(m_hash.data(), hash.data(), 32);
    }

    void DeleteNiece(const uint8_t lr)
    {
        if (m_nieces[lr] && m_nieces[lr]->m_aunt == this) {
            m_nieces[lr]->m_aunt = nullptr;
            delete m_nieces[lr];
            m_nieces[lr] = nullptr;
        }
    }

    void ReHash(const InternalNode<Hash>& sibling)
    {
        assert(sibling.m_nieces[0] && sibling.m_nieces[1]);
        m_hash = ParentHash(sibling.m_nieces[0]->m_hash, sibling.m_nieces[1]->m_hash);
    }

    bool IsDeadEnd() const { return !m_nieces[0] && !m_nieces[1]; }
};

template <typename Hash>
struct NodeAndMetadata {
private:
    //! Pointer to the node
    InternalNode<Hash>* m_node;
    //! The node's position.
    uint64_t m_position;
    //! Whether or not the node has a memorable child.
    bool m_has_memorable_child{false};
    //! Index of the root of which the node is a descendant of.
    uint8_t m_root_index{0};

public:
    NodeAndMetadata(InternalNode<Hash>& node, const uint64_t pos, const bool has_memorable_child, const uint8_t root_index)
        : m_node(&node), m_position(pos), m_has_memorable_child(has_memorable_child), m_root_index(root_index) {}

    InternalNode<Hash>& GetNode() const { return *m_node; }
    uint64_t GetPosition() const { return m_position; }
    bool HasMemorableChild() const { return m_has_memorable_child; }
    uint8_t GetRootIndex() const { return m_root_index; }

    NodeAndMetadata<Hash>& operator=(NodeAndMetadata<Hash> other)
    {
        std::swap(m_node, other.m_node);
        std::swap(m_position, other.m_position);
        std::swap(m_has_memorable_child, other.m_has_memorable_child);
        std::swap(m_root_index, other.m_root_index);
        return *this;
    }
};

template <typename Hash>
static bool CompareNodeAndMetadataByPosition(const NodeAndMetadata<Hash>& a, const NodeAndMetadata<Hash>& b)
{
    return a.GetPosition() < b.GetPosition();
}

/** Return the number of trailing one bits in n. */
static uint8_t NumTrailingOnes(uint64_t n)
{
    uint64_t b = ~n & (n + 1);
    --b;
    b = (b & 0x5555555555555555) +
        ((b >> 1) & 0x5555555555555555);
    b = (b & 0x3333333333333333) +
        ((b >> 2) & 0x3333333333333333);
    b = (b & 0x0f0f0f0f0f0f0f0f) +
        ((b >> 4) & 0x0f0f0f0f0f0f0f0f);
    b = (b & 0x00ff00ff00ff00ff) +
        ((b >> 8) & 0x00ff00ff00ff00ff);
    b = (b & 0x0000ffff0000ffff) +
        ((b >> 16) & 0x0000ffff0000ffff);
    b = (b & 0x00000000ffffffff) +
        ((b >> 32) & 0x00000000ffffffff);

    return b;
}
inline static uint8_t NumTrailingZeros(uint64_t n)
{
    return NumTrailingOnes(~n);
}

class ForestState
{
public:
    uint64_t m_root_bits;

    ForestState() : m_root_bits(0) {}
    ForestState(uint64_t n) : m_root_bits(n) {}

    // Functions to compute positions:
    uint64_t Parent(uint64_t pos) const
    {
        return (pos >> 1ULL) | (1ULL << NumRows());
    }
    uint64_t Ancestor(uint64_t pos, uint8_t rise) const
    {
        if (rise == 0) {
            return pos;
        }

        uint8_t rows{NumRows()};
        uint64_t mask{MaxNodes()};
        return (pos >> rise | (mask << (rows - (rise - 1)))) & mask;
    }
    uint64_t LeftChild(uint64_t pos) const
    {
        return (pos << 1) & MaxNodes();
    }
    uint64_t Child(uint64_t pos, uint64_t placement) const
    {
        return this->LeftChild(pos) | placement;
    }
    uint64_t LeftDescendant(uint64_t pos, uint8_t drop) const
    {
        if (drop == 0) {
            return pos;
        }

        uint64_t mask = MaxNodes();
        return (pos << drop) & mask;
    }
    uint64_t Cousin(uint64_t pos) const { return pos ^ 2; }
    uint64_t RightSibling(uint64_t pos) const { return pos | 1; }
    uint64_t Sibling(uint64_t pos) const { return pos ^ 1; }

    /**
     * Compute the path to the position. Return the index of the tree the
     * position is in, the distance from the node to its root and the bitfield
     * indicating the path.
     */
    std::tuple<uint8_t, uint8_t, uint64_t> Path(uint64_t pos) const
    {
        uint8_t rows{NumRows()};
        uint8_t row{DetectRow(pos)};

        uint8_t biggerTrees{0};
        for (; ((pos << row) & ((2ULL << rows) - 1)) >= ((1ULL << rows) & m_root_bits);
             --rows) {
            uint64_t treeSize = (1ULL << rows) & m_root_bits;
            if (treeSize != 0) {
                pos -= treeSize;
                ++biggerTrees;
            }
        }

        return std::make_tuple(biggerTrees, rows - row, ~pos);
    }

    std::vector<uint64_t> SimpleProofPositions(const std::vector<uint64_t>& targets) const
    {
        std::set<uint64_t> proof_positions;
        for (uint64_t target : targets) {
            if (IsRoot(target)) continue;
            proof_positions.emplace(Sibling(target));
        }

        std::vector<uint64_t> result;
        while (!proof_positions.empty()) {
            auto proof_pos_it{proof_positions.begin()};
            uint64_t pos{*proof_pos_it};
            proof_positions.erase(proof_pos_it);

            if (IsRoot(pos)) continue;

            if (proof_positions.erase(Sibling(pos)) == 0) {
                result.push_back(pos);
            }

            const uint64_t parent_pos{Parent(pos)};
            if (IsRoot(parent_pos)) continue;
            proof_positions.emplace(Sibling(parent_pos));
        }

        return result;
    }

    // Functions for root stuff:
    uint8_t NumRoots() const
    {
        std::bitset<64> bits(this->m_root_bits);
        return bits.count();
    }

    bool HasRoot(uint8_t row) const
    {
        return (m_root_bits >> row) & 1;
    }

    uint64_t RootPosition(uint8_t row) const
    {
        uint8_t rows{NumRows()};
        uint64_t mask = (2ULL << rows) - 1;
        uint64_t before = m_root_bits & (mask << (row + 1));
        uint64_t shifted = (before >> row) | (mask << (rows + 1 - row));
        return shifted & mask;
    }

    std::vector<uint64_t> RootPositions() const
    {
        std::vector<uint64_t> roots;
        for (uint8_t row = NumRows(); row >= 0 && row < MAX_TREE_HEIGHT; --row) {
            if (HasRoot(row)) {
                roots.push_back(RootPosition(row));
            }
        }
        return roots;
    }

    uint8_t RootIndex(uint64_t pos) const
    {
        uint8_t root_index{0};
        std::tie(root_index, std::ignore, std::ignore) = Path(pos);
        return root_index;
    }

    bool IsRoot(uint64_t pos) const
    {
        uint8_t row{DetectRow(pos)};
        if (!HasRoot(row)) {
            return false;
        }

        return RootPosition(row) == pos;
    }

    // Functions for rows:

    uint8_t NumRows() const
    {
        uint64_t n = m_root_bits;
        --n;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        ++n;

        return uint8_t(NumTrailingZeros(n) & ~64ULL);
    }
    uint8_t DetectRow(uint64_t pos) const
    {
        uint64_t marker = 1ULL << this->NumRows();
        uint8_t row = 0;

        for (; (pos & marker) != 0; ++row) {
            marker >>= 1;
        }

        return row;
    }

    // Return the maximum number of nodes in the forest.
    uint64_t MaxNodes() const { return (2ULL << NumRows()) - 1; }
};

template <typename Hash>
class AccumulatorImpl : public Accumulator<Hash>
{
public:
    using Leaves = std::vector<std::pair<Hash, bool>>;
    using Targets = std::vector<uint64_t>;
    using Hashes = std::vector<Hash>;
    ForestState m_current_state{0};

    std::vector<InternalNode<Hash>*> m_roots;

    std::map<Hash, InternalNode<Hash>*> m_cached_leaves;

    AccumulatorImpl(const uint64_t num_leaves, const std::vector<Hash>& roots)
    {
        m_current_state = ForestState(num_leaves);
        m_roots.reserve(64);
        for (const Hash& root_hash : roots) {
            m_roots.push_back(new InternalNode<Hash>(root_hash));
        }
    }
    AccumulatorImpl() = delete;
    AccumulatorImpl(AccumulatorImpl<Hash>&) = delete;
    AccumulatorImpl(AccumulatorImpl<Hash>&&) = delete;

    virtual ~AccumulatorImpl()
    {
        m_cached_leaves.clear();
        while (!m_roots.empty()) {
            InternalNode<Hash>* root{m_roots.back()};
            m_roots.pop_back();
            delete root;
        }
    }

    InternalNode<Hash>* WriteNode(uint64_t pos, const Hash& hash, bool allow_overwrite = false)
    {
        // Get the path to the position.
        const auto [tree, path_length, path_bits] = m_current_state.Path(pos);
        assert(tree < m_roots.size());

        InternalNode<Hash>* sibling{m_roots[tree]};
        assert(sibling);

        if (path_length == 0) {
            if (allow_overwrite) sibling->SetHash(hash);
            if (CompareHashes(sibling->m_hash, hash)) return sibling;
            return nullptr;
        }

        uint8_t lr_cutoff;
        InternalNode<Hash>* cutoff{nullptr};

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

        InternalNode<Hash>* node{GuaranteeNiece(*sibling, pos & 1)};
        if (!allow_overwrite && !CompareHashes(node->m_hash, NULL_HASH) && node->m_hash != hash) {
            // A hash already exists here and we are trying to replace it with
            // a different one. This should not be allowed.
            if (cutoff) cutoff->DeleteNiece(lr_cutoff);
            return nullptr;
        }

        node->SetHash(hash);

        return node;
    }
    InternalNode<Hash>* ReadNode(uint64_t pos) const
    {
        // TODO add check that the position can exist in m_current_state.

        // Get the path to the position.
        const auto [tree, path_length, path_bits] = m_current_state.Path(pos);
        assert(tree < m_roots.size());

        InternalNode<Hash>* node{m_roots[tree]};
        const InternalNode<Hash>* sibling{node};

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
        InternalNode<Hash>* node{ReadNode(pos)};
        if (node) {
            return std::optional<const Hash>{node->m_hash};
        }

        return std::nullopt;
    }

    /** Calcaulate a node's position by going up using the aunt pointers. */
    uint64_t ComputePosition(const InternalNode<Hash>& node) const
    {
        uint64_t path{0};
        uint8_t path_length{0};

        const InternalNode<Hash>* current{&node};
        while (path_length < MAX_TREE_HEIGHT) {
            const InternalNode<Hash>* aunt{current->m_aunt};
            if (!aunt) break;

            assert(aunt->m_nieces[0] == current || aunt->m_nieces[1] == current);
            const uint8_t lr{aunt->m_nieces[0] == current ? (uint8_t)0 : (uint8_t)1};

            path <<= 1;
            path |= path_length == 0 ? lr : lr ^ 1;

            current = aunt;
            ++path_length;
        }

        auto root_it = std::find_if(m_roots.cbegin(), m_roots.cend(), [&current](const InternalNode<Hash>* root) {
            return root == current;
        });
        assert(root_it != m_roots.cend());

        int root_index = std::distance(m_roots.cbegin(), root_it);
        assert(root_index < m_current_state.RootPositions().size());

        uint64_t position{m_current_state.RootPositions()[root_index]};
        for (int i = 0; i < path_length; ++i) {
            position = m_current_state.Child(position, path & 1);

            path >>= 1;
        }

        return position;
    }

    /** Mark a leaf as memorable by storing it in m_cached_leaves. */
    void MarkLeafAsMemorable(InternalNode<Hash>* node)
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
    void OverwriteMemorableMarker(const Hash& hash, InternalNode<Hash>* new_node)
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
    void MaybePruneNieces(InternalNode<Hash>& node)
    {
        bool left_niece_memorable{node.m_nieces[0] && HasMemorableMarker(node.m_nieces[0]->m_hash)};
        bool right_niece_memorable{node.m_nieces[1] && HasMemorableMarker(node.m_nieces[1]->m_hash)};

        if (node.m_nieces[0] && node.m_nieces[0]->IsDeadEnd() &&
            !left_niece_memorable && !right_niece_memorable) node.DeleteNiece(0);
        if (node.m_nieces[1] && node.m_nieces[1]->IsDeadEnd() &&
            !right_niece_memorable && !left_niece_memorable) node.DeleteNiece(1);
    }
    /** Go up using the aunt pointers and prune. */
    void PruneBranch(InternalNode<Hash>& leaf)
    {
        InternalNode<Hash>* aunt{leaf.m_aunt};
        if (!aunt) {
            // The leaf is a root and there is nothing else todo besides
            // removing the memorable marker.
            return;
        }
        const uint8_t lr_sib{aunt->m_nieces[0] == &leaf ? (uint8_t)1 : (uint8_t)0};
        InternalNode<Hash>* sibling{aunt->m_nieces[lr_sib]};
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
    void SetAuntForNieces(InternalNode<Hash>& aunt)
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
    InternalNode<Hash>* GuaranteeNiece(InternalNode<Hash>& aunt, const uint8_t lr_niece)
    {
        if (!aunt.m_nieces[lr_niece]) {
            // This will be pruned away since we dont set its nieces.
            // TODO can we do without the extra allocation?
            aunt.m_nieces[lr_niece] = new InternalNode<Hash>(NULL_HASH);
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
    InternalNode<Hash>* GuaranteeParent(const InternalNode<Hash>& node, uint64_t parent_pos)
    {
        InternalNode<Hash>* aunt{node.m_aunt};
        // Roots do not have parents
        if (!aunt) return nullptr;

        InternalNode<Hash>* grand_aunt{aunt->m_aunt};
        // Aunt is a root
        if (!grand_aunt) return aunt;

        return GuaranteeNiece(*grand_aunt, parent_pos & 1);
    }


    /** Rehash and prune a branch until a root is reached. */
    void ReHashToTop(InternalNode<Hash>& node, InternalNode<Hash>& sibling, const uint64_t node_pos)
    {
        if (sibling.m_nieces[0] && sibling.m_nieces[1]) {
            node.ReHash(sibling);
            MaybePruneNieces(sibling);
        }

        InternalNode<Hash>* aunt{node.m_aunt};
        if (!aunt) {
            // node is a root
            assert(m_current_state.HasRoot(m_current_state.DetectRow(node_pos)));
            assert(m_current_state.RootPosition(m_current_state.DetectRow(node_pos)) == node_pos);
            return;
        }

        const uint64_t parent_pos{m_current_state.Parent(node_pos)};
        InternalNode<Hash>* grand_aunt{aunt->m_aunt};
        if (!grand_aunt) {
            // aunt is a root.
            ReHashToTop(*aunt, *aunt, parent_pos);
            return;
        }

        uint8_t lr_parent{(uint8_t)(parent_pos & 1)};
        InternalNode<Hash>* parent{GuaranteeNiece(*grand_aunt, lr_parent)};
        ReHashToTop(*parent, *aunt, parent_pos);
    }

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
    void AddLeaf(const Hash& hash, const bool remember)
    {
        InternalNode<Hash>* new_root{new InternalNode<Hash>(hash)};
        if (remember) {
            MarkLeafAsMemorable(new_root);
        }

        for (uint8_t row = 0; row < MAX_TREE_HEIGHT && m_current_state.HasRoot((uint8_t)row); ++row) {
            assert(m_roots.size() > 0);

            InternalNode<Hash>* left_niece = m_roots.back();
            m_roots.pop_back();
            assert(new_root && left_niece);

            if (CompareHashes(left_niece->m_hash, ZOMBIE_ROOT_HASH)) {
                delete left_niece;
                continue;
            }

            std::swap(left_niece->m_nieces, new_root->m_nieces);
            SetAuntForNieces(*left_niece);
            SetAuntForNieces(*new_root);

            new_root = new InternalNode<Hash>(left_niece, new_root);
            SetAuntForNieces(*new_root);
            MaybePruneNieces(*new_root);
        }

        m_roots.push_back(new_root);
        m_current_state = ForestState(m_current_state.m_root_bits + 1);
    }

    /** Add multiple leaves to the accumulator by adding each leaf individually. */
    void Add(const std::vector<std::pair<Hash, bool>>& new_leaves)
    {
        for (auto leaf = new_leaves.cbegin(); leaf < new_leaves.cend(); ++leaf) {
            AddLeaf(/*hash=*/leaf->first, /*remember=*/leaf->second);
        }
    }

    /**
     * Promotes the sibling of the aunt's `lr_node` (0 = left, 1 = right) niece
     * to the parent, effectively removing the niece.
     */
    void PromoteSibling(InternalNode<Hash>& parent, InternalNode<Hash>& aunt, const uint8_t lr_node)
    {
        const uint8_t lr_sib{(uint8_t)(lr_node ^ 1)};
        const InternalNode<Hash>* sibling{aunt.m_nieces[lr_sib]};
        assert(sibling);

        if (HasMemorableMarker(sibling->m_hash)) {
            OverwriteMemorableMarker(sibling->m_hash, &parent);
        }

        // To remove a leaf we promote its sibling to be its parent.
        parent.SetHash(sibling->m_hash);

        InternalNode<Hash>* node{aunt.m_nieces[lr_node]};
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

    /**
     * Remove a single leaf/node by promoting its sibling to be its parent.
     *
     * Return whether or not the node was a root.
     */
    void RemoveOne(const InternalNode<Hash>& sibling, const uint64_t pos)
    {
        InternalNode<Hash>* aunt{sibling.m_aunt};
        if (!aunt) {
            // Mark the root as deleted by replacing its hash with the ZOMBIE_ROOT_HASH.
            uint8_t root_index{m_current_state.RootIndex(pos)};
            m_roots[root_index]->SetHash(ZOMBIE_ROOT_HASH);
            m_roots[root_index]->DeleteNiece(0);
            m_roots[root_index]->DeleteNiece(1);
            return;
        }

        InternalNode<Hash>* grand_aunt{aunt->m_aunt};
        if (!grand_aunt) {
            // The aunt is a root, so the node we want to delete is its
            // child as well as its niece.
            PromoteSibling(/*parent=*/*aunt, /*aunt=*/*aunt, /*lr_node=*/pos & 1);
            return;
        }

        uint8_t lr_parent{aunt == grand_aunt->m_nieces[0] ? (uint8_t)1 : (uint8_t)0};
        InternalNode<Hash>* parent{GuaranteeNiece(*grand_aunt, lr_parent)};
        assert(parent);

        PromoteSibling(/*parent=*/*parent, /*aunt=*/*aunt, /*lr_node=*/pos & 1);

        ReHashToTop(*parent, *aunt, m_current_state.Parent(pos));
    }

    /**
     * Remove multiple leaves given their positions.
     *
     * Removing leaves will fail if one or more of the leaves are not cached.
     * To remove a leaf its inclusion proof has to be cached with in the
     * accumulator or the accumulator could not be updated to exclude the leaf.
     *
     * Return whether or not removing *all* leaves succeeded.
     */
    bool Remove(const std::vector<uint64_t>& targets)
    {
        if (targets.size() == 0) return true;

        // Hashes of the leaves that we want to remove.
        std::vector<Hash> target_hashes;
        target_hashes.reserve(targets.size());

        // Siblings of the leaves that we want to remove.
        std::map<uint64_t, InternalNode<Hash>*> target_siblings;
        for (auto pos = targets.cbegin(); pos < targets.cend(); ++pos) {
            // The leaves we want to remove (targets) have to be cached.
            InternalNode<Hash>* target_node{ReadNode(*pos)};
            if (!target_node) return false;

            auto leaf_it = m_cached_leaves.find(target_node->m_hash);
            if (leaf_it == m_cached_leaves.end()) return false;
            target_hashes.push_back(target_node->m_hash);

            // The siblings of the leaves we want to remove have to be cached.
            InternalNode<Hash>* sibling{target_node->m_aunt ? ReadNode(m_current_state.Sibling(*pos)) : target_node};
            if (!sibling) return false;

            uint64_t sibling_pos{target_node->m_aunt ? m_current_state.Sibling(*pos) : *pos};
            target_siblings.emplace(sibling_pos, sibling);
        }

        while (!target_siblings.empty()) {
            auto it{target_siblings.begin()};
            const uint64_t pos{it->first};
            InternalNode<Hash>* sibling{it->second};
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

    bool IngestProof(std::map<uint64_t, InternalNode<Hash>*>& verification_map, const BatchProof<Hash>& proof, const std::vector<Hash>& target_hashes)
    {
        if (proof.GetTargets().size() != target_hashes.size()) return false;

        std::vector<uint64_t> proof_positions = m_current_state.SimpleProofPositions(proof.GetSortedTargets());
        if (proof_positions.size() != proof.GetHashes().size()) return false;

        // Write targets
        for (int i = 0; i < proof.GetTargets().size(); ++i) {
            const uint64_t pos{proof.GetSortedTargets()[i]};
            const Hash& hash{target_hashes[i]};

            InternalNode<Hash>* new_node = WriteNode(pos, hash);
            if (!new_node) return false;

            verification_map.emplace(pos, new_node);
        }

        // Write proof
        for (int i = 0; i < proof.GetHashes().size(); ++i) {
            const uint64_t pos{proof_positions[i]};
            const Hash& hash{proof.GetHashes()[i]};

            InternalNode<Hash>* proof_node = WriteNode(pos, hash);
            if (!proof_node) return false;

            verification_map.emplace(pos, proof_node);
        }

        return true;
    }

    /** Implement `Accumulator` interface */

    bool Verify(const BatchProof<Hash>& proof, const std::vector<Hash>& target_hashes) override
    {
        // TODO sanity checks

        // Ingest the proof, popualting the accumulator with target and proof
        // hashes. `IngestProof` will not overwrite existing hashes and an
        // existing hash has to match the passed one for the population to
        // succeed.
        std::map<uint64_t, InternalNode<Hash>*> unverified;
        bool ingest_ok{IngestProof(unverified, proof, target_hashes)};

        // Mark all newly inserted leaves as memorable. This is necessary so
        // that they are later provable using `Prove`.
        std::queue<InternalNode<Hash>*> new_leaves;
        for (int i = 0; i < target_hashes.size() && ingest_ok; ++i) {
            bool was_cached{HasMemorableMarker(target_hashes[i])};
            MarkLeafAsMemorable(unverified[proof.GetSortedTargets()[i]]);

            if (!was_cached) {
                new_leaves.push(m_cached_leaves.at(target_hashes[i]));
            }
        }

        bool verify_ok{ingest_ok};
        while (verify_ok && !unverified.empty()) {
            auto node_it{unverified.begin()};
            uint64_t node_pos{node_it->first};
            const InternalNode<Hash>* node{node_it->second};

            unverified.erase(node_it);

            if (!node->m_aunt) {
                // This is a root target. We can ignore it here, `IngestProof` made
                // sure that the root hash was not overwritten.
                continue;
            }

            uint64_t sibling_pos{m_current_state.Sibling(node_pos)};
            auto sibling_it{unverified.find(sibling_pos)};
            if (sibling_it == unverified.end()) {
                verify_ok = false;
                break;
            }
            const InternalNode<Hash>* sibling{sibling_it->second};

            unverified.erase(sibling_it);

            InternalNode<Hash>* parent{GuaranteeParent(*node, m_current_state.Parent(node_pos))};
            if (!parent) {
                verify_ok = false;
                break;
            }

            // Quick hack to sort the children for hashing.
            const InternalNode<Hash>* children[2];
            children[node_pos & 1] = node;
            children[sibling_pos & 1] = sibling;

            Hash computed_hash = ParentHash(children[0]->m_hash, children[1]->m_hash);
            if (!CompareHashes(parent->m_hash, NULL_HASH) && parent->m_hash != computed_hash) {
                // The parent was cached and the newly computed hash did not match the
                // cached one, so verification fails.
                verify_ok = false;
                break;
            }

            if (parent->m_aunt) {
                // Don't add roots to the unverified set.
                unverified.emplace(m_current_state.Parent(node_pos), parent);
            }

            parent->SetHash(computed_hash);

            MaybePruneNieces(*node->m_aunt);
        }

        if (!verify_ok) {
            while (!new_leaves.empty()) {
                InternalNode<Hash>* leaf{new_leaves.front()};
                new_leaves.pop();

                // Remove memorable marker.
                RemoveMemorableMarkerFromLeaf(leaf->m_hash);
                // We need to prune away all newly inserted nodes in the
                // accumulator if verification fails, so that invalid hashes are
                // not inserted into the accumulator.
                PruneBranch(*leaf);
            }
            return false;
        }

        return true;
    }

    bool Modify(const std::vector<std::pair<Hash, bool>>& new_leaves,
                const std::vector<uint64_t>& targets) override
    {
        if (!Remove(targets)) return false;
        Add(new_leaves);
        assert(m_roots.size() == m_current_state.NumRoots());
        return true;
    }

    void UndoOne(InternalNode<Hash>& parent, InternalNode<Hash>& aunt, const uint8_t lr_sib) {}
    bool Undo(const uint64_t previous_num_leaves,
              const Hashes& previous_root_hashes,
              const BatchProof<Hash>& previous_proof,
              const Hashes& previous_targets) override
    {
        // TODO
        return false;
    }

    bool Prove(BatchProof<Hash>& proof, const std::vector<Hash>& target_hashes) const override
    {
        if (target_hashes.size() == 0) return true;

        std::map<uint64_t, const InternalNode<Hash>*> proof_nodes;
        std::vector<uint64_t> targets;

        // Get all the siblings of the targets.
        for (auto hash = target_hashes.cbegin(); hash < target_hashes.cend(); ++hash) {
            auto leaf_it = m_cached_leaves.find(*hash);
            if (leaf_it == m_cached_leaves.end()) return false;

            InternalNode<Hash>* leaf_node{leaf_it->second};
            const uint64_t leaf_pos{ComputePosition(*leaf_node)};
            targets.push_back(leaf_pos);

            if (!leaf_node->m_aunt) {
                // Roots have no proof.
                continue;
            }

            const uint64_t sibing_pos{m_current_state.Sibling(leaf_pos)};
            const InternalNode<Hash>* sibling{ReadNode(sibing_pos)};
            if (!sibling) return false;
            assert(sibling->IsDeadEnd());

            proof_nodes.emplace(sibing_pos, sibling);
        }

        std::vector<Hash> proof_hashes;

        // Fetch all nodes contained in the proof by going upwards.
        while (!proof_nodes.empty()) {
            auto node_it{proof_nodes.begin()};
            uint64_t node_pos{node_it->first};
            const InternalNode<Hash>* node{node_it->second};
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
    void Uncache(const Hash& leaf_hash) override
    {
        auto leaf_it = m_cached_leaves.find(leaf_hash);

        // If this leaf isn't cached, we can quit early.
        if (leaf_it == m_cached_leaves.end()) return;

        InternalNode<Hash>* leaf{leaf_it->second};

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
        std::vector<NodeAndMetadata<Hash>> leaves_and_positions;
        for (const auto& [hash, node] : m_cached_leaves) {
            leaves_and_positions.emplace_back(*node, ComputePosition(*node), false, 0);
        }

        std::sort(leaves_and_positions.begin(), leaves_and_positions.end(), CompareNodeAndMetadataByPosition<Hash>);

        std::vector<Hash> leaf_hashes;
        for (const NodeAndMetadata<Hash>& leaf_and_pos : leaves_and_positions) {
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
        return {m_current_state.m_root_bits, root_hashes};
    }

    void Serialize(std::vector<uint8_t>& result) const override
    {
        std::vector<uint8_t> root_bit_bytes(8);
        WriteBE64(root_bit_bytes.data(), m_current_state.m_root_bits);
        result.insert(result.end(), root_bit_bytes.begin(), root_bit_bytes.end());

        std::stack<InternalNode<Hash>*> stack;
        for (auto rit = m_roots.rbegin(); rit != m_roots.rend(); ++rit) {
            stack.push(*rit);
        }

        while (!stack.empty()) {
            InternalNode<Hash>* node{stack.top()};
            stack.pop();

            uint8_t node_metadata = (node->m_nieces[0] ? (uint8_t)0b001 : (uint8_t)0) |
                                    (node->m_nieces[1] ? (uint8_t)0b010 : (uint8_t)0) |
                                    (IsCached(node->m_hash) ? (uint8_t)0b100 : (uint8_t)0);
            result.push_back(node_metadata);

            std::array<uint8_t, 32> hash;
            std::memcpy(hash.data(), node->m_hash.data(), 32);
            result.insert(result.end(), hash.begin(), hash.end());

            if (node->m_nieces[1]) stack.push(node->m_nieces[1]);
            if (node->m_nieces[0]) stack.push(node->m_nieces[0]);
        }
    }

    InternalNode<Hash>* UnserializeSubTree(const std::vector<uint8_t>& bytes, uint64_t& data_offset)
    {
        uint8_t node_metadata{bytes[data_offset]};
        data_offset += 1;
        if (node_metadata > 7) throw std::runtime_error("failed to unserialize accumulator");

        Hash hash;
        std::memcpy(hash.data(), bytes.data() + data_offset, 32);
        data_offset += 32;

        InternalNode<Hash>* node = new InternalNode<Hash>(hash);

        bool has_left_niece{(node_metadata & 1) > 0};
        bool has_right_niece{(node_metadata & 2) > 0};
        bool is_cached_leaf{(node_metadata & 4) > 0};
        if (has_left_niece) {
            node->m_nieces[0] = UnserializeSubTree(bytes, data_offset);
        }
        if (has_right_niece) {
            node->m_nieces[1] = UnserializeSubTree(bytes, data_offset);
        }
        if (is_cached_leaf) {
            MarkLeafAsMemorable(node);
        }

        SetAuntForNieces(*node);

        return node;
    }
    void Unserialize(const std::vector<uint8_t>& bytes) override
    {
        assert(m_roots.size() == 0);

        uint64_t data_offset{0};
        m_current_state.m_root_bits = ReadBE64(bytes.data());
        data_offset += 8;

        while (data_offset < bytes.size()) {
            m_roots.push_back(UnserializeSubTree(bytes, data_offset));
        }
    }
};

} // namespace detail

template <typename Hash>
inline static std::unique_ptr<Accumulator<Hash>> Make(const uint64_t root_bits, const std::vector<Hash>& roots)
{
    return std::make_unique<detail::AccumulatorImpl<Hash>>(root_bits, roots);
}
template <typename Hash>
inline static std::unique_ptr<Accumulator<Hash>> MakeEmpty()
{
    return Make<Hash>(0, {});
}

} // namespace utreexo

#endif
