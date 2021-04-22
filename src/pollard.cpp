#include "../include/pollard.h"
#include "../include/batchproof.h"
#include "node.h"
#include "nodepool.h"
#include "state.h"
#include <deque>
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

    InternalNode() {}
    ~InternalNode() {}

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

    void NodePoolDestroy()
    {
        m_nieces[0] = nullptr;
        m_nieces[1] = nullptr;
    }
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
    ~Node() {}

    const Hash& GetHash() const override;
    void ReHash() override;

    void NodePoolDestroy() override
    {
        m_verification_flag = 0;
        m_node = nullptr;
        m_sibling = nullptr;
        Accumulator::Node::NodePoolDestroy();
    }

    bool IsTargetOrAncestor() const { return m_verification_flag & TARGET; }
    bool IsValid() const { return m_verification_flag & VALID; }
    bool IsCached() const { return m_verification_flag & CACHED; }
    bool IsSiblingCached() const { return m_verification_flag & SIBLING_CACHED; }
    void MarkAsValid() { m_verification_flag |= VALID; }
};

// Pollard
Pollard::Pollard(uint64_t num_leaves, int max_nodes) : Accumulator(num_leaves)
{
    // TODO: find good capacity for both pools.
    m_int_nodepool = new NodePool<Pollard::InternalNode>(max_nodes);
    m_nodepool = new NodePool<Pollard::Node>(max_nodes);
    m_remember = new NodePtr<Pollard::InternalNode>(m_int_nodepool);
}

Pollard::~Pollard()
{
    m_roots.clear();
    delete m_remember;
    delete m_int_nodepool;
    delete m_nodepool;
}

std::vector<Accumulator::NodePtr<Pollard::InternalNode>> Pollard::Read(uint64_t pos, NodePtr<Accumulator::Node>& rehash_path, bool record_path) const
{
    ForestState current_state(m_num_leaves);

    std::vector<NodePtr<Pollard::InternalNode>> family;
    family.reserve(2);

    // Get the path to the position.
    uint8_t tree, path_length;
    uint64_t path_bits;
    std::tie(tree, path_length, path_bits) = current_state.Path(pos);

    // There is no node above a root.
    rehash_path = nullptr;

    NodePtr<Pollard::InternalNode> node = INTERNAL_NODE(m_roots[tree]),
                                   sibling = INTERNAL_NODE(m_roots[tree]);
    uint64_t node_pos = current_state.RootPositions()[tree];

    if (path_length == 0) {
        family.push_back(node);
        family.push_back(sibling);
        return family;
    }


    // Traverse the pollard until the desired position is reached.
    for (uint8_t i = 0; i < path_length; ++i) {
        uint8_t lr = (path_bits >> (path_length - 1 - i)) & 1;
        uint8_t lr_sib = current_state.Sibling(lr);

        if (record_path) {
            auto path_node = NodePtr<Pollard::Node>(m_nodepool);
            path_node->m_num_leaves = m_num_leaves;
            path_node->m_position = node_pos;
            path_node->m_node = node;
            path_node->m_parent = rehash_path;
            path_node->m_sibling = sibling;
            rehash_path = path_node;
        }

        node = sibling->m_nieces[lr_sib];
        sibling = sibling->m_nieces[lr];

        node_pos = current_state.Child(node_pos, lr_sib);
    }

    family.push_back(node);
    family.push_back(sibling);
    return family;
}

Accumulator::NodePtr<Accumulator::Node> Pollard::SwapSubTrees(uint64_t from, uint64_t to)
{
    NodePtr<Accumulator::Node> rehash_path, unused;
    std::vector<NodePtr<InternalNode>> family_from, family_to;
    family_from = this->Read(from, unused, false);
    family_to = this->Read(to, rehash_path, true);

    NodePtr<InternalNode> node_from, sibling_from,
        node_to, sibling_to;
    node_from = family_from.at(0);
    sibling_from = family_from.at(1);
    node_to = family_to.at(0);
    sibling_to = family_to.at(1);

    // Swap the hashes of node a and b.
    std::swap(node_from->m_hash, node_to->m_hash);
    // Swap the nieces of the siblings of a and b.
    std::swap(sibling_from->m_nieces, sibling_to->m_nieces);

    return rehash_path;
}

Accumulator::NodePtr<Accumulator::Node> Pollard::NewLeaf(const Leaf& leaf)
{
    auto int_node = NodePtr<InternalNode>(m_int_nodepool);
    int_node->m_hash = leaf.first;

    assert(m_remember);
    int_node->m_nieces[0] = leaf.second ? *m_remember : nullptr;
    int_node->m_nieces[1] = nullptr;

    auto node = NodePtr<Pollard::Node>(m_nodepool);
    node->m_num_leaves = m_num_leaves;
    node->m_position = m_num_leaves;
    node->m_node = int_node;
    node->m_sibling = int_node;
    node->m_parent = nullptr;
    m_roots.push_back(node);

    return m_roots.back();
}

Accumulator::NodePtr<Accumulator::Node> Pollard::MergeRoot(uint64_t parent_pos, Hash parent_hash)
{
    auto right = m_roots.back();
    NodePtr<InternalNode> int_right = INTERNAL_NODE(m_roots.back());
    m_roots.pop_back();
    NodePtr<InternalNode> int_left = INTERNAL_NODE(m_roots.back());
    m_roots.pop_back();

    // swap nieces
    std::swap(int_left->m_nieces, int_right->m_nieces);

    // create internal node
    auto int_node = NodePtr<InternalNode>(m_int_nodepool);
    int_node->m_hash = parent_hash;
    int_node->m_nieces[0] = int_left;
    int_node->m_nieces[1] = int_right;
    int_node->Prune();

    auto node = NodePtr<Pollard::Node>(m_nodepool);
    node->m_num_leaves = m_num_leaves;
    node->m_position = parent_pos;
    node->m_node = int_node;
    node->m_sibling = int_node;
    node->m_parent = nullptr;
    m_roots.push_back(node);

    return m_roots.back();
}

void Pollard::FinalizeRemove(uint64_t next_num_leaves)
{
    ForestState current_state(m_num_leaves), next_state(next_num_leaves);

    // Compute the positions of the new roots in the current state.
    std::vector<uint64_t> new_positions = current_state.RootPositions(next_state.m_num_leaves);

    // Select the new roots.
    std::vector<NodePtr<Accumulator::Node>> new_roots;
    new_roots.reserve(new_positions.size());

    for (uint64_t new_pos : new_positions) {
        NodePtr<Accumulator::Node> unused_path;
        std::vector<NodePtr<InternalNode>> family = this->Read(new_pos, unused_path, false);

        // TODO: the forest state of these root nodes should reflect the new state
        // since they survive the remove op.
        auto node = NodePtr<Pollard::Node>(m_nodepool);
        node->m_num_leaves = current_state.m_num_leaves;
        node->m_position = new_pos;
        node->m_node = family.at(0);
        node->m_sibling = node->m_node;
        node->m_parent = nullptr;
        new_roots.push_back(node);
    }

    m_roots.clear();
    m_roots = new_roots;
}

bool Pollard::Prove(BatchProof& proof, const std::vector<Hash>& target_hashes) const
{
    // TODO: Add ability to prove cached leaves.
    return false;
}

void Pollard::Prune()
{
    for (NodePtr<Accumulator::Node>& root : m_roots) {
        INTERNAL_NODE(root)->Chop();
    }
    assert(m_remember->RefCount() == 1);
}

void Pollard::InitChildrenOfComputed(Accumulator::NodePtr<Pollard::Node>& node,
                                     Accumulator::NodePtr<Pollard::Node>& left_child,
                                     Accumulator::NodePtr<Pollard::Node>& right_child,
                                     bool& recover_left,
                                     bool& recover_right)
{
    if (!node->m_sibling->m_nieces[0]) {
        // The left child does not exist in the pollard. We need to hook it in.
        Accumulator::NodePtr<Pollard::InternalNode> int_left(m_int_nodepool);
        int_left->m_hash.fill(0);
        node->m_sibling->m_nieces[0] = int_left;
        recover_left = true;
    }

    if (!node->m_sibling->m_nieces[1]) {
        // The right child does not exist in the pollard. We need to hook it in.
        Accumulator::NodePtr<Pollard::InternalNode> int_right(m_int_nodepool);
        int_right->m_hash.fill(0);
        node->m_sibling->m_nieces[1] = int_right;
        recover_right = true;
    }

    // TODO: create macros or something for these inits.
    left_child->m_parent = node;
    left_child->m_node = node->m_sibling->m_nieces[0];
    left_child->m_sibling = node->m_sibling->m_nieces[1];
    left_child->m_position = ForestState(m_num_leaves).LeftChild(node->m_position);
    if (!recover_left) left_child->m_verification_flag |= Pollard::Node::CACHED;
    if (!recover_right) left_child->m_verification_flag |= Pollard::Node::SIBLING_CACHED;
    left_child->m_num_leaves = m_num_leaves;

    right_child->m_parent = node;
    right_child->m_node = node->m_sibling->m_nieces[1];
    right_child->m_sibling = node->m_sibling->m_nieces[0];
    right_child->m_position = ForestState(m_num_leaves).Child(node->m_position, 1);
    if (!recover_left) right_child->m_verification_flag |= Pollard::Node::SIBLING_CACHED;
    if (!recover_right) right_child->m_verification_flag |= Pollard::Node::CACHED;
    right_child->m_num_leaves = m_num_leaves;
}

bool Pollard::CreateProofTree(std::vector<Accumulator::NodePtr<Pollard::Node>>& blaze,
                              std::vector<std::pair<Accumulator::NodePtr<Pollard::Node>, int>>& recovery,
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
    std::deque<Accumulator::NodePtr<Pollard::Node>> blaze_tree;

    int row = static_cast<int>(state.NumRows());

    // For each row in the forest, we populate the proof tree.
    while (row >= 0) {
        // For storing the next row of the proof tree.
        std::deque<Accumulator::NodePtr<Pollard::Node>> next_row;

        // Roots are the entry points to the forest from the top down.
        // We attach the root to the current row if there is one on this row.
        if (computed_pos < computed_positions.crend() &&
            state.HasRoot(row) && *computed_pos == state.RootPosition(row)) {
            Accumulator::NodePtr<Accumulator::Node> root = m_roots.at(state.RootIndex(*computed_pos));
            // TODO: make the roots have the correct positions before this.
            root->m_position = state.RootPosition(row);
            blaze_tree.push_back(root);
            blaze_tree.back()->m_verification_flag =
                Pollard::Node::CACHED | Pollard::Node::SIBLING_CACHED;
        }

        // Iterate over the proof tree in reverse and for each node:
        // - check that the node is a proof or a computed node.
        // - if it is a computed node we prepend its children to the next row.
        // - if it is a proof node we populate it with the provided proof hash.
        // (Because we go in reverse we are able to adjust for missing proof hashes based on what is cached)
        for (auto node_it = blaze_tree.rbegin(); node_it != blaze_tree.rend(); ++node_it) {
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
                // in the blaze tree.
                Accumulator::NodePtr<Pollard::Node> left_child(m_nodepool);
                Accumulator::NodePtr<Pollard::Node> right_child(m_nodepool);
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

                // Prepend the children to next row of the blaze tree.
                next_row.push_front(right_child);
                next_row.push_front(left_child);
            } else if (is_proof) {
                // This node is a proof node we populate with the provided hash.
                // We might not consume a hash from the proof if the node is cached.
                // Proof nodes dont have children in the blaze tree, so we dont add any node to
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

        blaze_tree = next_row;
        --row;
    }

    std::copy(blaze_tree.begin(), blaze_tree.end(), std::inserter(blaze, blaze.begin()));

    return proof_hash == proof.GetHashes().crend();
}


bool Pollard::VerifyProofTree(std::vector<Accumulator::NodePtr<Pollard::Node>> blaze_tree,
                              const std::vector<Hash>& target_hashes,
                              const std::vector<Hash>& proof_hashes)
{
    bool verification_success = true;
    auto target_hash = target_hashes.begin();

    while (blaze_tree.size() > 0 && verification_success) {
        std::vector<NodePtr<Pollard::Node>> next_blaze_tree;

        // Iterate over the current blaze tree row.
        for (NodePtr<Pollard::Node>& blaze : blaze_tree) {
            // TODO: get rid of leaf proof nodes
            if (!blaze->IsTargetOrAncestor()) continue;

            // ==================================================
            // This node is a target or the ancestor of a target.

            NodePtr<Pollard::Node> parent = blaze->Parent();
            // If this node is valid, so is its parent.
            if (parent && blaze->IsValid()) parent->MarkAsValid();

            // Append the parent to the next blaze tree row, if it exists.
            // (A root does not have a parent.)
            NodePtr<Pollard::Node> last_parent = next_blaze_tree.size() > 0 ?
                                                     next_blaze_tree.back() :
                                                     nullptr;
            if (parent &&
                (!last_parent || last_parent != parent)) {
                next_blaze_tree.push_back(parent);
            }

            bool is_leaf = blaze->m_position < m_num_leaves;
            if (is_leaf) {
                assert(target_hash < target_hashes.end());

                // ======================
                // This node is a target.
                // Either populate with target hash or verify that the hash matches if cached.
                if (blaze->IsCached()) {
                    verification_success = blaze->m_node->m_hash == *target_hash;
                    // Mark the parent as valid if this is not a leaf root.
                    if (verification_success && parent && blaze->IsSiblingCached()) parent->MarkAsValid();
                } else {
                    blaze->m_node->m_hash = *target_hash;
                }


                ++target_hash;
                continue;
            }

            // ==================================
            // This node is an ancestor of a target.

            if (!blaze->IsValid() && blaze->IsCached()) {
                // This node is cached but not marked as valid (e.g.: a root) => we have to verify that the computed hash
                // matches the cached one.
                // Higher nodes on this branch are all valid.
                verification_success = blaze->ReHashAndVerify();
                if (!verification_success) break;

                // Mark the parent as valid if it exists.
                // We do this to avoid hashing higher up on this branch.
                if (parent && blaze->IsSiblingCached()) parent->MarkAsValid();
            } else if (blaze->IsValid() && blaze->IsCached()) {
            } else {
                // This node was not cached => we compute its hash from its children.
                blaze->ReHashNoPrune();
                // This node has to to have a parent because it cant be a root.
                assert(parent);
            }
        }

        blaze_tree = next_blaze_tree;
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

    int prev_internal_nodes = m_int_nodepool->Size();

    // The blaze tree holds the leaves of the partial tree involved in verifying the proof.
    // The leaves know their parents, so the tree can be traversed from the bottom up.
    std::vector<NodePtr<Pollard::Node>> blaze_tree;

    // The recovery tree is needed in case the proof is invalid.
    // It holds the intersection nodes where new nodes have been populated.
    // In case of verification failure we chop the tree down at these nodes
    // to prevent mutating the pollard.
    using IntersectionNode = std::pair<NodePtr<Pollard::Node>, int>;
    std::vector<IntersectionNode> recovery_tree;

    // Populate the blaze tree from top to bottom.
    // This adds new empty nodes to the pollard that will either hold
    // proof hashes or hashes that were computed during verification.
    bool create_ok = CreateProofTree(blaze_tree, recovery_tree, proof);

    int prev_verify_internal_nodes = m_int_nodepool->Size();

    // Verify the blaze tree from bottom to top.
    // This is were the "blaze" metaphore comes from because we "burn" the tree.
    bool verify_ok = create_ok && VerifyProofTree(blaze_tree, target_hashes, proof.GetHashes());
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
            default:
                assert(false);
            }
        }

        // Clear the recovery and blaze tree, so the references to the internal nodes
        // are removed.
        recovery_tree.clear();
        blaze_tree.clear();

        // Ensure that the number of allocated internal nodes matches the
        // number of nodes before the verification.
        assert(prev_internal_nodes == m_int_nodepool->Size());
        return false;
    }

    // TODO: in theory the blaze could be used during deletion as well.
    // it has references to all nodes that get swaped around. Using the blaze
    // tree could make deletion more efficient since we would not have to traverse
    // the pollard to find the node that need to be swapped.
    blaze_tree.clear();

    // Ensure that the number of internal nodes does not change during verification.
    assert(prev_verify_internal_nodes == m_int_nodepool->Size());
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
