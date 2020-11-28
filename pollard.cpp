#include <algorithm>
#include <crypto/sha256.h>
#include <iostream>
#include <pollard.h>
#include <string.h>

// Get the internal node from a NodePtr<Accumulator::Node>.
#define INTERNAL_NODE(acc_node) (((Pollard::Node*)acc_node.get())->m_node)

// Pollard

std::vector<NodePtr<Pollard::InternalNode>> Pollard::Read(uint64_t pos, NodePtr<Accumulator::Node>& rehash_path, bool record_path) const
{
    std::vector<NodePtr<Pollard::InternalNode>> family;
    family.reserve(2);

    // Get the path to the position.
    uint8_t tree, path_length;
    uint64_t path_bits;
    std::tie(tree, path_length, path_bits) = this->m_state.Path(pos);

    // There is no node above a root.
    rehash_path = nullptr;

    NodePtr<Pollard::InternalNode> node = INTERNAL_NODE(this->m_roots[tree]),
                                   sibling = INTERNAL_NODE(this->m_roots[tree]);
    uint64_t node_pos = this->m_state.RootPositions()[tree];

    if (path_length == 0) {
        family.push_back(node);
        family.push_back(sibling);
        return family;
    }


    // Traverse the pollard until the desired position is reached.
    for (uint8_t i = 0; i < path_length; ++i) {
        uint8_t lr = (path_bits >> (path_length - 1 - i)) & 1;
        uint8_t lr_sib = this->m_state.Sibling(lr);

        //auto int_node = new Pollard::Node(this->m_state, node_pos, node, rehash_path, sibling);
        if (record_path) {
            auto path_node = NodePtr<Pollard::Node>(m_nodepool);
            path_node->m_forest_state = m_state;
            path_node->m_position = node_pos;
            path_node->m_node = node;
            path_node->m_parent = rehash_path;
            path_node->m_sibling = sibling;
            rehash_path = path_node;
        }

        node = sibling->m_nieces[lr_sib];
        sibling = sibling->m_nieces[lr];

        node_pos = this->m_state.Child(node_pos, lr_sib);
    }

    family.push_back(node);
    family.push_back(sibling);
    return family;
}

NodePtr<Accumulator::Node> Pollard::SwapSubTrees(uint64_t from, uint64_t to)
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

NodePtr<Accumulator::Node> Pollard::NewLeaf(uint256& hash)
{
    auto int_node = NodePtr<InternalNode>(m_int_nodepool);
    int_node->m_hash = hash;
    //memcpy(int_node->m_hash.begin(), hash.begin(), 32);
    // TODO: dont remember everything
    int_node->m_nieces[0] = int_node;
    int_node->m_nieces[1] = nullptr;

    auto node = NodePtr<Pollard::Node>(m_nodepool);
    node->m_forest_state = m_state;
    node->m_position = m_state.m_num_leaves;
    node->m_node = int_node;
    this->m_roots.push_back(node);

    return this->m_roots.back();
}

NodePtr<Accumulator::Node> Pollard::MergeRoot(uint64_t parent_pos, uint256 parent_hash)
{
    NodePtr<InternalNode> int_right = INTERNAL_NODE(this->m_roots.back());
    this->m_roots.pop_back();
    NodePtr<InternalNode> int_left = INTERNAL_NODE(this->m_roots.back());
    this->m_roots.pop_back();
    // swap nieces
    std::swap(int_left->m_nieces, int_right->m_nieces);

    // create internal node
    auto int_node = NodePtr<InternalNode>(m_int_nodepool);
    //memcpy(int_node->m_hash.begin(), parent_hash.begin(), 32);
    int_node->m_hash = parent_hash;
    int_node->m_nieces[0] = int_left;
    int_node->m_nieces[1] = int_right;
    int_node->Prune();

    auto node = NodePtr<Pollard::Node>(m_nodepool);
    node->m_forest_state = m_state;
    node->m_position = parent_pos;
    node->m_node = int_node;
    this->m_roots.push_back(node);

    return this->m_roots.back();
}

void Pollard::FinalizeRemove(const ForestState next_state)
{
    // Compute the positions of the new roots in the current state.
    std::vector<uint64_t> new_positions = this->m_state.RootPositions(next_state.m_num_leaves);

    // Select the new roots.
    std::vector<NodePtr<Accumulator::Node>> new_roots;
    new_roots.reserve(new_positions.size());

    for (uint64_t new_pos : new_positions) {
        NodePtr<Accumulator::Node> unused_path;
        std::vector<NodePtr<InternalNode>> family = this->Read(new_pos, unused_path, false);

        auto node = NodePtr<Pollard::Node>(m_nodepool);
        node->m_forest_state = m_state;
        node->m_position = new_pos;
        node->m_node = family.at(0);
        new_roots.push_back(node);
    }

    m_roots.clear();
    this->m_roots = new_roots;
}

// Pollard::Node

const uint256& Pollard::Node::Hash() const
{
    return m_node.get()->m_hash;
}

void Pollard::Node::ReHash()
{
    if (!this->m_sibling->m_nieces[0] || !this->m_sibling->m_nieces[1]) {
        // TODO: error could not rehash one of the children is not known.
        // This will happen if there are duplicates in the dirtyNodes in Accumulator::Remove.
        return;
    }

    this->m_node->m_hash = Accumulator::ParentHash(this->m_sibling->m_nieces[0]->m_hash, this->m_sibling->m_nieces[1]->m_hash);
    this->m_sibling->Prune();
}

// Pollard::InternalNode

void Pollard::InternalNode::Prune()
{
    if (!this->m_nieces[0] || this->m_nieces[0]->DeadEnd()) {
        this->m_nieces[0] = nullptr;
    }

    if (!this->m_nieces[1] || this->m_nieces[1]->DeadEnd()) {
        this->m_nieces[1] = nullptr;
    }
}

bool Pollard::InternalNode::DeadEnd() const
{
    return !this->m_nieces[0] && !this->m_nieces[1];
}
