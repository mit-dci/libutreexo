#ifndef UTREEXO_POLLARD_H
#define UTREEXO_POLLARD_H

#include <accumulator.h>
#include <memory>
#include <nodepool.h>

namespace utreexo {

class Pollard : public Accumulator
{
private:
    class InternalNode
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

    /* Pollards implementation of Accumulator::Node */
    class Node : public Accumulator::Node
    {
    public:
        NodePtr<InternalNode> m_node;

        // Store the sibling for reHash.
        // The siblings nieces are the nodes children.
        NodePtr<InternalNode> m_sibling;

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

    NodePool<InternalNode>* m_int_nodepool;
    NodePool<Pollard::Node>* m_nodepool;

    /*
     * Return the node and its sibling.
     * Point path to the parent of the node. The path to the node can be traversed in reverse order using the
     * Accumulator::Node::Parent function.
     */
    std::vector<NodePtr<Pollard::InternalNode>> Read(uint64_t pos, NodePtr<Accumulator::Node>& path, bool record_path) const;

    NodePtr<Accumulator::Node> SwapSubTrees(uint64_t from, uint64_t to) override;
    NodePtr<Accumulator::Node> MergeRoot(uint64_t parent_pos, Hash parent_hash) override;
    NodePtr<Accumulator::Node> NewLeaf(const Leaf& hash) override;
    void FinalizeRemove(uint64_t next_num_leaves) override;

public:
    Pollard(uint64_t num_leaves, int max_nodes) : Accumulator(num_leaves)
    {
        // TODO: find good capacity for both pools.
        m_int_nodepool = new NodePool<Pollard::InternalNode>(max_nodes);
        m_nodepool = new NodePool<Pollard::Node>(max_nodes);
    }

    ~Pollard()
    {
        m_roots.clear();
        delete m_int_nodepool;
        delete m_nodepool;
    }

    bool Prove(BatchProof& proof, const std::vector<Hash>& target_hashes) const override;
};

};     // namespace utreexo
#endif // UTREEXO_POLLARD_H
