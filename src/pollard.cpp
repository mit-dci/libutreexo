#include "../include/pollard.h"
#include "node.h"
#include "nodepool.h"
#include "state.h"
#include <string.h>
#include <tuple>

// Get the internal node from a NodePtr<Accumulator::Node>.
#define INTERNAL_NODE(acc_node) (((Pollard::Node*)acc_node.get())->m_node)

namespace utreexo {

class Pollard::InternalNode
{
public:
    Hash m_hash;
    NodePtr<InternalNode> m_nieces[2];

    InternalNode() {}
    ~InternalNode() {}

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
public:
    NodePtr<Pollard::InternalNode> m_node;

    // Store the sibling for reHash.
    // The siblings nieces are the nodes children.
    NodePtr<Pollard::InternalNode> m_sibling;

    Node() {}
    ~Node() {}

    const Hash& GetHash() const override;
    void ReHash() override;

    void NodePoolDestroy() override
    {
        m_node = nullptr;
        m_sibling = nullptr;
        Accumulator::Node::NodePoolDestroy();
    }
};

// Pollard
Pollard::Pollard(uint64_t num_leaves, int max_nodes) : Accumulator(num_leaves)
{
    // TODO: find good capacity for both pools.
    m_int_nodepool = new NodePool<Pollard::InternalNode>(max_nodes);
    m_nodepool = new NodePool<Pollard::Node>(max_nodes);
}

Pollard::~Pollard()
{
    m_roots.clear();
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
    // TODO: dont remember everything
    int_node->m_nieces[0] = int_node;
    int_node->m_nieces[1] = nullptr;

    auto node = NodePtr<Pollard::Node>(m_nodepool);
    node->m_num_leaves = m_num_leaves;
    node->m_position = m_num_leaves;
    node->m_node = int_node;
    m_roots.push_back(node);

    return m_roots.back();
}

Accumulator::NodePtr<Accumulator::Node> Pollard::MergeRoot(uint64_t parent_pos, Hash parent_hash)
{
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
        new_roots.push_back(node);
    }

    m_roots.clear();
    m_roots = new_roots;
}

bool Pollard::Prove(BatchProof& proof, const std::vector<Hash>& target_hashes) const
{
    return false;
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

// Pollard::InternalNode

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
