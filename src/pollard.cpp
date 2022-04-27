#include "../include/pollard.h"
#include "../include/batchproof.h"
#include "check.h"
#include "node.h"
#include "state.h"
#include <deque>
#include <memory>
#include <optional>
#include <string.h>
#include <tuple>

// Get the internal node from a NodePtr<Accumulator::Node>.
#define INTERNAL_NODE(acc_node) (((Pollard::Node*)acc_node.get())->m_node)

namespace utreexo {

// Recovery flags constants:
static const int RECOVERY_CHOP_LEFT = 0;
static const int RECOVERY_CHOP_RIGHT = 1;
static const int RECOVERY_CHOP_BOTH = 2;

class Pollard::InternalNode
{
public:
    Hash m_hash;
    NodePtr<InternalNode> m_nieces[2];

    InternalNode() : InternalNode(nullptr, nullptr) {}
    InternalNode(NodePtr<InternalNode> left, NodePtr<InternalNode> right)
    {
        m_nieces[0] = left;
        m_nieces[1] = right;
        m_hash.fill(0);
    }
    InternalNode(NodePtr<InternalNode> left, NodePtr<InternalNode> right, const Hash& hash)
    {
        m_nieces[0] = left;
        m_nieces[1] = right;
        m_hash = hash;
    }

    ~InternalNode()
    {
        m_nieces[0] = nullptr;
        m_nieces[1] = nullptr;
    }

    /* Chop of nieces */
    void Chop();
    void ChopLeft();
    void ChopRight();

    /* Chop of deadend nieces. */
    void Prune();

    /*
     * Return wether or not this node is a deadend.
     * A node is a deadend if both nieces do not point to another node.
     */
    bool DeadEnd() const;
};

class Pollard::Node : public Accumulator::Node
{
private:
    bool ReHashAndVerify() const;

    void ReHashNoPrune();

public:
    friend class Pollard;

    // Verification flags:
    // A valid node has the correct hash.
    static const int VALID = 1;
    // This marks a node as target or ancestor of a target.
    static const int TARGET = 1 << 1;
    // This marks a node as cached.
    static const int CACHED = 1 << 2;
    // The sibling of a node is cached.
    // Roots have both the CACHED and SIBLING_CACHED bit set.
    static const int SIBLING_CACHED = 1 << 3;

    NodePtr<Pollard::InternalNode> m_node;

    // Store the sibling for reHash.
    // The siblings nieces are the nodes children.
    NodePtr<Pollard::InternalNode> m_sibling;

    uint8_t m_verification_flag{0};

    Node() {}
    Node(NodePtr<Pollard::InternalNode> node,
         NodePtr<Pollard::InternalNode> sibling,
         NodePtr<Accumulator::Node> parent,
         uint64_t num_leaves,
         uint64_t pos)
        : m_node(node), m_sibling(sibling), m_verification_flag(0)
    {
        m_parent = parent;
        m_num_leaves = num_leaves;
        m_position = pos;
    }

    ~Node()
    {
        m_verification_flag = 0;
        m_node = nullptr;
        m_sibling = nullptr;
    }

    const Hash& GetHash() const override;
    void ReHash() override;

    bool IsTargetOrAncestor() const { return m_verification_flag & TARGET; }
    bool IsValid() const { return m_verification_flag & VALID; }
    bool IsCached() const { return m_verification_flag & CACHED; }
    bool IsSiblingCached() const { return m_verification_flag & SIBLING_CACHED; }
    void MarkAsValid() { m_verification_flag |= VALID; }
};

// Pollard
Pollard::Pollard(uint64_t num_leaves) : Accumulator(num_leaves)
{
    m_remember = std::make_shared<InternalNode>();
}

Pollard::Pollard(const std::vector<Hash>& roots, uint64_t num_leaves)
    : Pollard(num_leaves)
{
    ForestState state(m_num_leaves);

    assert(roots.size() == state.NumRoots());
    auto root_positions = state.RootPositions();
    assert(root_positions.size() == roots.size());

    // Restore roots
    for (int i = 0; i < roots.size(); ++i) {
        auto int_node = MakeNodePtr<InternalNode>(nullptr, nullptr, roots.at(i));
        m_roots.push_back(MakeNodePtr<Pollard::Node>(int_node, int_node, nullptr,
                                                     m_num_leaves, root_positions.at(i)));
    }
}

Pollard::~Pollard()
{
    m_roots.clear();
}

std::optional<const Hash> Pollard::Read(uint64_t pos) const
{
    auto [node, sibling] = ReadSiblings(pos);
    if (!node) {
        return std::nullopt;
    }

    return std::optional<const Hash>{node->m_hash};
}

std::vector<Hash> Pollard::ReadLeafRange(uint64_t pos, uint64_t range) const
{
    // TODO: implement efficient way of reading these hashes from the range,
    // without trying to read every position.
    std::vector<Hash> hashes;
    for (uint64_t i = pos; i < pos + range; ++i) {
        std::optional<const Hash> hash = Read(i);
        if (hash.has_value()) {
            hashes.push_back(Read(i).value());
        }
    }
    return hashes;
}

Pollard::InternalSiblings Pollard::ReadSiblings(uint64_t pos, NodePtr<Accumulator::Node>& rehash_path, bool record_path) const
{
    const ForestState current_state(m_num_leaves);

    // Get the path to the position.
    const auto [tree, path_length, path_bits] = current_state.Path(pos);

    // There is no node above a root.
    rehash_path = nullptr;

    uint64_t node_pos = current_state.RootPositions()[tree];
    NodePtr<Pollard::InternalNode> node = INTERNAL_NODE(m_roots[tree]);
    NodePtr<Pollard::InternalNode> sibling = node;

    if (path_length == 0) {
        // Roots act as their own sibling.
        return {node, sibling};
    }

    // Traverse the pollard until the desired position is reached.
    for (uint8_t i = 0; i < path_length; ++i) {
        uint8_t lr = (path_bits >> (path_length - 1 - i)) & 1;
        uint8_t lr_sib = current_state.Sibling(lr);

        if (record_path) {
            rehash_path = Accumulator::MakeNodePtr<Pollard::Node>(node, sibling, rehash_path,
                                                                  m_num_leaves, node_pos);
        }

        if (!sibling) {
            return {nullptr, nullptr};
        }

        node = sibling->m_nieces[lr_sib];
        sibling = sibling->m_nieces[lr];

        node_pos = current_state.Child(node_pos, lr_sib);
    }

    return {node, sibling};
}

Pollard::InternalSiblings Pollard::ReadSiblings(uint64_t pos) const
{
    NodePtr<Accumulator::Node> unused;
    return ReadSiblings(pos, unused, false);
}

NodePtr<Accumulator::Node> Pollard::SwapSubTrees(uint64_t from, uint64_t to)
{
    ForestState state(m_num_leaves);

    NodePtr<Accumulator::Node> rehash_path;

    auto hook_in_sibling = [&rehash_path, state](NodePtr<InternalNode>& sibling, uint64_t pos) {
        if (!sibling) {
            uint8_t lr = pos & 1;
            uint8_t lr_sib = state.Sibling(lr);
            sibling = Accumulator::MakeNodePtr<InternalNode>();
            std::dynamic_pointer_cast<Pollard::Node>(rehash_path)->m_sibling->m_nieces[lr_sib] = sibling;
        }
    };

    auto [node_from, sibling_from] = ReadSiblings(from, rehash_path, true);
    CHECK_SAFE(node_from);
    hook_in_sibling(sibling_from, from);

    NodePtr<InternalNode> node_to{nullptr}, sibling_to{nullptr};
    if (state.Sibling(from) == to) {
        node_to = sibling_from;
        sibling_to = node_from;
    } else {
        std::tie(node_to, sibling_to) = ReadSiblings(to, rehash_path, true);
        CHECK_SAFE(node_to);
        hook_in_sibling(sibling_to, to);
    }

    std::swap(node_to->m_hash, node_from->m_hash);
    std::swap(sibling_to->m_nieces, sibling_from->m_nieces);

    return rehash_path;
}

NodePtr<Accumulator::Node> Pollard::NewLeaf(const Leaf& leaf)
{
    assert(m_remember);
    NodePtr<InternalNode> int_node = Accumulator::MakeNodePtr<InternalNode>(
        leaf.second ? m_remember : nullptr, nullptr, leaf.first);

    NodePtr<Pollard::Node> node = Accumulator::MakeNodePtr<Pollard::Node>(
        /*node*/ int_node, /*sibling*/ int_node, /*parent*/ nullptr,
        m_num_leaves, m_num_leaves);
    m_roots.push_back(node);

    // Only keep the hash in the map if the leaf is marked to be
    // remembered.
    if (leaf.second) {
        m_posmap[leaf.first] = node->m_position;
    }

    return m_roots.back();
}

NodePtr<Accumulator::Node> Pollard::MergeRoot(uint64_t parent_pos, Hash parent_hash)
{
    auto right = m_roots.back();
    NodePtr<InternalNode> int_right = INTERNAL_NODE(m_roots.back());
    m_roots.pop_back();
    NodePtr<InternalNode> int_left = INTERNAL_NODE(m_roots.back());
    m_roots.pop_back();

    // swap nieces
    std::swap(int_left->m_nieces, int_right->m_nieces);

    // create internal node
    NodePtr<InternalNode> int_node = Accumulator::MakeNodePtr<InternalNode>(int_left, int_right, parent_hash);
    int_node->Prune();

    NodePtr<Pollard::Node> node = Accumulator::MakeNodePtr<Pollard::Node>(
        /*node*/ int_node, /*sibling*/ int_node, /*parent*/ nullptr,
        m_num_leaves, parent_pos);
    m_roots.push_back(node);

    return m_roots.back();
}

void Pollard::FinalizeRemove(uint64_t next_num_leaves)
{
    ForestState current_state(m_num_leaves), next_state(next_num_leaves);

    // Remove deleted leaf hashes from the position map.
    for (uint64_t pos = next_state.m_num_leaves; pos < current_state.m_num_leaves; ++pos) {
        if (std::optional<const Hash> read_hash = Read(pos)) {
            m_posmap.erase(read_hash.value());
        }
    }

    // Compute the positions of the new roots in the current state.
    std::vector<uint64_t> new_positions = current_state.RootPositions(next_state.m_num_leaves);

    // Select the new roots.
    std::vector<NodePtr<Accumulator::Node>> new_roots(new_positions.size());
    int new_root_index = new_roots.size() - 1;

    while (new_root_index >= 0) {
        uint64_t new_pos = new_positions.at(new_root_index);

        auto [int_node, int_sibling] = ReadSiblings(new_pos);
        CHECK_SAFE(int_node);

        // TODO: the forest state of these root nodes should reflect the new state
        // since they survive the remove op.
        NodePtr<Pollard::Node> node = Accumulator::MakeNodePtr<Pollard::Node>(
            int_node, int_node, nullptr,
            current_state.m_num_leaves, new_pos);

        // When truning a node into a root, it's nieces are really it's children
        if (int_sibling) {
            node->m_node->m_nieces[0] = int_sibling->m_nieces[0];
            node->m_node->m_nieces[1] = int_sibling->m_nieces[1];
        } else {
            node->m_node->Chop();
        }

        new_roots[new_root_index] = node;
        --new_root_index;
    }

    m_roots.clear();
    m_roots = new_roots;
}

void Pollard::Prune()
{
    for (NodePtr<Accumulator::Node>& root : m_roots) {
        INTERNAL_NODE(root)->Chop();
    }
    assert(m_remember.use_count() == 1);
}

uint64_t Pollard::CountNodes(const NodePtr<Pollard::InternalNode>& node) const
{
    if (!node || node == m_remember) return 0;
    return 1 + CountNodes(node->m_nieces[0]) + CountNodes(node->m_nieces[1]);
}

uint64_t Pollard::CountNodes() const
{
    uint64_t res = 0;
    for (auto root : m_roots) {
        res += CountNodes(INTERNAL_NODE(root));
    }
    return res;
}

void Pollard::InitChildrenOfComputed(NodePtr<Pollard::Node>& node,
                                     NodePtr<Pollard::Node>& left_child,
                                     NodePtr<Pollard::Node>& right_child,
                                     bool& recover_left,
                                     bool& recover_right)
{
    recover_left = false;
    recover_right = false;

    if (!node->m_sibling->m_nieces[0]) {
        // The left child does not exist in the pollard. We need to hook it in.
        node->m_sibling->m_nieces[0] = Accumulator::MakeNodePtr<InternalNode>();
        recover_left = true;
    }

    if (!node->m_sibling->m_nieces[1]) {
        // The right child does not exist in the pollard. We need to hook it in.
        node->m_sibling->m_nieces[1] = Accumulator::MakeNodePtr<InternalNode>();
        recover_right = true;
    }

    left_child = Accumulator::MakeNodePtr<Pollard::Node>(
        /*node*/ node->m_sibling->m_nieces[0], /*sibling*/ node->m_sibling->m_nieces[1], /*parent*/ node,
        m_num_leaves, ForestState(m_num_leaves).LeftChild(node->m_position));
    if (!recover_left) left_child->m_verification_flag |= Pollard::Node::CACHED;
    if (!recover_right) left_child->m_verification_flag |= Pollard::Node::SIBLING_CACHED;

    right_child = Accumulator::MakeNodePtr<Pollard::Node>(
        /*node*/ node->m_sibling->m_nieces[1], /*sibling*/ node->m_sibling->m_nieces[0], /*parent*/ node,
        m_num_leaves, ForestState(m_num_leaves).Child(node->m_position, 1));
    if (!recover_left) right_child->m_verification_flag |= Pollard::Node::SIBLING_CACHED;
    if (!recover_right) right_child->m_verification_flag |= Pollard::Node::CACHED;
}

bool Pollard::CreateProofTree(std::vector<NodePtr<Pollard::Node>>& proof_tree_out,
                              std::vector<std::pair<NodePtr<Pollard::Node>, int>>& recovery,
                              const BatchProof& proof)
{
    ForestState state(m_num_leaves);
    std::vector<uint64_t> proof_positions, computed_positions;
    std::tie(proof_positions, computed_positions) = state.ProofPositions(proof.GetSortedTargets());

    auto proof_hash = proof.GetHashes().crbegin();
    auto proof_pos = proof_positions.crbegin();
    auto computed_pos = computed_positions.crbegin();

    // We use a std::deque here because we need to be able to append and prepend efficiently
    // and a vector does not offer that.
    // TODO: use std::deque in all of the verification logic?
    std::deque<NodePtr<Pollard::Node>> proof_tree;

    int row = static_cast<int>(state.NumRows());

    // For each row in the forest, we populate the proof tree.
    while (row >= 0) {
        // For storing the next row of the proof tree.
        std::deque<NodePtr<Pollard::Node>> next_row;

        // Roots are the entry points to the forest from the top down.
        // We attach the root to the current row if there is one on this row.
        if (computed_pos < computed_positions.crend() &&
            state.HasRoot(row) && *computed_pos == state.RootPosition(row)) {
            NodePtr<Accumulator::Node> root = m_roots.at(state.RootIndex(*computed_pos));
            // TODO: make the roots have the correct positions before this.
            root->m_position = state.RootPosition(row);
            proof_tree.push_back(std::dynamic_pointer_cast<Pollard::Node>(root));
            proof_tree.back()->m_verification_flag =
                Pollard::Node::CACHED | Pollard::Node::SIBLING_CACHED;
        }

        // Iterate over the proof tree in reverse and for each node:
        // - check that the node is a proof or a computed node.
        // - if it is a computed node we prepend its children to the next row.
        // - if it is a proof node we populate it with the provided proof hash.
        // (Because we go in reverse we are able to adjust for missing proof hashes based on what is cached)
        for (auto node_it = proof_tree.rbegin(); node_it != proof_tree.rend(); ++node_it) {
            NodePtr<Pollard::Node>& node = *node_it;

            // Populate next row
            if (node->m_position < m_num_leaves) {
                // This is a leaf.
                next_row.push_front(node);
            }

            bool is_computed = computed_pos < computed_positions.crend() &&
                               *computed_pos == node->m_position;
            bool is_proof = proof_pos < proof_positions.crend() &&
                            *proof_pos == node->m_position;

            // Ensure that this node is either part of the proof or will be computed.
            assert(!(is_proof && is_computed));
            assert(is_proof || is_computed);

            if (is_computed) {
                ++computed_pos;
                node->m_verification_flag |= Pollard::Node::TARGET;

                if (node->m_position < m_num_leaves) continue;

                // Since this is computed node it must have two children
                // in the proof tree.
                NodePtr<Pollard::Node> left_child = nullptr;
                NodePtr<Pollard::Node> right_child = nullptr;
                bool recover_left = false, recover_right = false;

                // Initialise the children of this computed node.
                // If the children dont exist in the pollard we need create and
                // insert them.
                InitChildrenOfComputed(node, left_child, right_child,
                                       recover_left, recover_right);

                // Remember which nodes were inserted for recovery purposes.
                if (recover_left && recover_right) {
                    // Both children were newly inserted into the pollard.
                    recovery.emplace_back(node, RECOVERY_CHOP_BOTH);
                } else if (recover_left) {
                    // Only the left child was inserted.
                    recovery.emplace_back(node, RECOVERY_CHOP_LEFT);
                } else if (recover_right) {
                    // Only the right child was inserted.
                    recovery.emplace_back(node, RECOVERY_CHOP_RIGHT);
                }

                // Prepend the children to next row of the proof tree.
                next_row.push_front(right_child);
                next_row.push_front(left_child);
            } else if (is_proof) {
                // This node is a proof node we populate with the provided hash.
                // We might not consume a hash from the proof if the node is cached.
                // Proof nodes dont have children in the proof tree, so we dont add any node to
                // the next row here.

                node->m_verification_flag &= ~(Pollard::Node::TARGET);
                ++proof_pos;

                // Populate the proof hashses.
                bool consume = true;
                if (node->IsCached()) {
                    Hash null_hash;
                    null_hash.fill(0);
                    const Hash& hash = proof_hash < proof.GetHashes().crend() ? *proof_hash : null_hash;
                    // If provided proof hash matches the cached hash, we consume the hash.
                    consume = node->m_node->m_hash == hash;
                } else {
                    // This proof was not cached.
                    // We populate with the provided proof hash.
                    // If the proof is invalid then this will lead to verification
                    // failure during the rehashing of the parent node.

                    if (proof_hash >= proof.GetHashes().crend()) {
                        // The needed proof hash was not supplied.
                        return false;
                    }

                    node->m_node->m_hash = *proof_hash;
                }

                if (consume) ++proof_hash;
            }
        }

        proof_tree = next_row;
        --row;
    }

    std::copy(proof_tree.begin(), proof_tree.end(), std::inserter(proof_tree_out, proof_tree_out.begin()));

    return proof_hash == proof.GetHashes().crend();
}


bool Pollard::VerifyProofTree(std::vector<NodePtr<Pollard::Node>> proof_tree,
                              const std::vector<Hash>& target_hashes,
                              const std::vector<Hash>& proof_hashes)
{
    bool verification_success = true;
    auto target_hash = target_hashes.begin();

    while (proof_tree.size() > 0 && verification_success) {
        std::vector<NodePtr<Pollard::Node>> next_proof_tree;

        // Iterate over the current proof tree row.
        for (NodePtr<Pollard::Node>& proof_node : proof_tree) {
            // TODO: get rid of leaf proof nodes
            if (!proof_node->IsTargetOrAncestor()) continue;

            // ==================================================
            // This node is a target or the ancestor of a target.

            NodePtr<Pollard::Node> parent =
                std::dynamic_pointer_cast<Pollard::Node>(proof_node->Parent());
            // If this node is valid, so is its parent.
            if (parent && proof_node->IsValid()) parent->MarkAsValid();

            // Append the parent to the next proof tree row, if it exists.
            // (A root does not have a parent.)
            NodePtr<Pollard::Node> last_parent = next_proof_tree.size() > 0 ?
                                                     next_proof_tree.back() :
                                                     nullptr;
            if (parent &&
                (!last_parent || last_parent != parent)) {
                next_proof_tree.push_back(parent);
            }

            bool is_leaf = proof_node->m_position < m_num_leaves;
            if (is_leaf) {
                if (target_hash == target_hashes.end()) {
                    return false;
                }

                CHECK_SAFE(proof_node->IsTargetOrAncestor());

                // ======================
                // This node is a target.
                // Either populate with target hash or verify that the hash matches if cached.
                if (proof_node->IsCached()) {
                    verification_success = proof_node->m_node->m_hash == *target_hash;
                    if (!verification_success) break;

                    CHECK_SAFE(proof_node->m_position == ForestState(m_num_leaves).RootPosition(0) ||
                               proof_node->m_node->m_nieces[0] == m_remember ||
                               m_posmap.find(*target_hash) != m_posmap.end());

                    // Mark the parent as valid if this is not a leaf root.
                    if (verification_success && parent && proof_node->IsSiblingCached()) parent->MarkAsValid();
                } else {
                    proof_node->m_node->m_hash = *target_hash;
                }

                proof_node->m_sibling->m_nieces[0] = m_remember;

                ++target_hash;
                continue;
            }

            // ==================================
            // This node is an ancestor of a target.

            if (!proof_node->IsValid() && proof_node->IsCached()) {
                // This node is cached but not marked as valid (e.g.: a root) => we have to verify that the computed hash
                // matches the cached one.
                // Higher nodes on this branch are all valid.
                verification_success = proof_node->ReHashAndVerify();
                if (!verification_success) break;

                // Mark the parent as valid if it exists.
                // We do this to avoid hashing higher up on this branch.
                if (parent && proof_node->IsSiblingCached()) parent->MarkAsValid();
            } else if (proof_node->IsValid() && proof_node->IsCached()) {
            } else {
                // This node was not cached => we compute its hash from its children.
                proof_node->ReHashNoPrune();
                // This node has to to have a parent because it cant be a root.
                assert(parent);
            }
        }

        proof_tree = next_proof_tree;
    }

    // Make sure all proof and target hashes were consumed.
    verification_success = verification_success &&
                           target_hash >= target_hashes.end();

    return verification_success;
}

bool Pollard::Verify(const BatchProof& proof, const std::vector<Hash>& target_hashes)
{
    // The number of targets specified in the proof must match the number of provided target hashes.
    if (target_hashes.size() != proof.GetTargets().size()) return false;

    // If the proof fails the sanity check it fails to verify.
    // (e.g.: the targets are not sorted)
    if (!proof.CheckSanity(m_num_leaves)) return false;

    // If there are no targets to verify we are done.
    if (proof.GetTargets().size() == 0) return true;

    // The proof tree holds the leaves of the partial tree involved in verifying the proof.
    // The leaves know their parents, so the tree can be traversed from the bottom up.
    std::vector<NodePtr<Pollard::Node>> proof_tree;

    // The recovery tree is needed in case the proof is invalid.
    // It holds the intersection nodes where new nodes have been populated.
    // In case of verification failure we chop the tree down at these nodes
    // to prevent mutating the pollard.
    using IntersectionNode = std::pair<NodePtr<Pollard::Node>, int>;
    std::vector<IntersectionNode> recovery_tree;

    // Populate the proof tree from top to bottom.
    // This adds new empty nodes to the pollard that will either hold
    // proof hashes or hashes that were computed during verification.
    bool create_ok = CreateProofTree(proof_tree, recovery_tree, proof);

    // Verify the proof tree from bottom to top.
    bool verify_ok = create_ok && VerifyProofTree(proof_tree, target_hashes, proof.GetHashes());
    if (!verify_ok) {
        // The proof was invalid and we have to revert the changes that were made to the pollard.
        // This is where we use the recovery tree to chop of the newly populated branches.
        // TODO: the recovery tree currently holds every node of a new branch. In theory we only
        // need the top most node to chop of the entire branch.
        for (IntersectionNode& intersection : recovery_tree) {
            assert(intersection.first);
            assert(intersection.first->m_sibling);

            // Chop of the newly created branches.
            // Since this is the pollard, the intersection's sibling has
            // the its children.
            switch (intersection.second) {
            case RECOVERY_CHOP_LEFT:
                intersection.first->m_sibling->ChopLeft();
                break;
            case RECOVERY_CHOP_RIGHT:
                intersection.first->m_sibling->ChopRight();
                break;
            case RECOVERY_CHOP_BOTH:
                intersection.first->m_sibling->Chop();
                break;
            }
        }

        // Clear the recovery and proof tree, so the references to the internal nodes
        // are removed.
        recovery_tree.clear();
        proof_tree.clear();

        return false;
    }

    // All targets are now remembered.
    for (int i = 0; i < target_hashes.size(); i++) {
        m_posmap[target_hashes[i]] = proof.GetSortedTargets()[i];
    }

    // TODO: in theory the proof tree could be used during deletion as well.
    // it has references to all nodes that get swaped around. Using the proof
    // tree could make deletion more efficient since we would not have to traverse
    // the pollard to find the node that need to be swapped.
    proof_tree.clear();

    // Proof verification passed.
    return true;
}

// Pollard::Node

const Hash& Pollard::Node::GetHash() const
{
    return m_node.get()->m_hash;
}

void Pollard::Node::ReHash()
{
    if (!m_sibling->m_nieces[0] || !m_sibling->m_nieces[1]) {
        // TODO: error could not rehash one of the children is not known.
        // This will happen if there are duplicates in the dirtyNodes in Accumulator::Remove.
        return;
    }

    Accumulator::ParentHash(m_node->m_hash, m_sibling->m_nieces[0]->m_hash, m_sibling->m_nieces[1]->m_hash);
    m_sibling->Prune();
}

void Pollard::Node::ReHashNoPrune()
{
    if (!m_sibling->m_nieces[0] || !m_sibling->m_nieces[1]) {
        // TODO: error could not rehash one of the children is not known.
        // This will happen if there are duplicates in the dirtyNodes in Accumulator::Remove.
        return;
    }

    Accumulator::ParentHash(m_node->m_hash, m_sibling->m_nieces[0]->m_hash, m_sibling->m_nieces[1]->m_hash);
}

bool Pollard::Node::ReHashAndVerify() const
{
    if (!m_sibling->m_nieces[0] || !m_sibling->m_nieces[1]) {
        // Leaves cant rehash and verify.
        return false;
    }

    if (IsValid()) {
        return true;
    }

    Hash computed_hash;
    Accumulator::ParentHash(computed_hash, m_sibling->m_nieces[0]->m_hash, m_sibling->m_nieces[1]->m_hash);
    return computed_hash == m_node->m_hash;
}

// Pollard::InternalNode

void Pollard::InternalNode::Chop()
{
    m_nieces[0] = nullptr;
    m_nieces[1] = nullptr;
}

void Pollard::InternalNode::ChopLeft()
{
    m_nieces[0] = nullptr;
}

void Pollard::InternalNode::ChopRight()
{
    m_nieces[1] = nullptr;
}

void Pollard::InternalNode::Prune()
{
    if (!m_nieces[0] || m_nieces[0]->DeadEnd()) {
        m_nieces[0] = nullptr;
    }

    if (!m_nieces[1] || m_nieces[1]->DeadEnd()) {
        m_nieces[1] = nullptr;
    }
}

bool Pollard::InternalNode::DeadEnd() const
{
    return !m_nieces[0] && !m_nieces[1];
}

}; // namespace utreexo
