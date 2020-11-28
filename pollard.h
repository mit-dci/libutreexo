#ifndef UTREEXO_POLLARD_H
#define UTREEXO_POLLARD_H

#include <accumulator.h>
#include <memory>
#include <nodepool.h>

class Pollard : public Accumulator
{
private:
    class InternalNode
    {
    public:
        uint256 m_hash;
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

    NodePool<InternalNode>* m_int_nodepool;

    /*
     * Return the node and its sibling.
     * Point path to the parent of the node. The path to the node can be traversed in reverse order using the
     * Accumulator::Node::Parent function.
     */
    std::vector<NodePtr<Pollard::InternalNode>> Read(uint64_t pos, NodePtr<Accumulator::Node>& path, bool record_path) const;

    ///protected:
    class Node : public Accumulator::Node
    {
    public:
        NodePtr<InternalNode> m_node;

        // Store the sibling for reHash.
        // The siblings nieces are the nodes children.
        NodePtr<InternalNode> m_sibling;

        Node() {}
        ~Node() {}

        const uint256& Hash() const override;
        void ReHash() override;

        void NodePoolDestroy() override
        {
            m_node = nullptr;
            m_sibling = nullptr;
            Accumulator::Node::NodePoolDestroy();
        }
    };

    NodePool<Pollard::Node>* m_nodepool;

    NodePtr<Accumulator::Node> SwapSubTrees(uint64_t from, uint64_t to) override;
    NodePtr<Accumulator::Node> MergeRoot(uint64_t parent_pos, uint256 parent_hash) override;
    NodePtr<Accumulator::Node> NewLeaf(uint256& hash) override;
    void FinalizeRemove(const ForestState next_state) override;

public:
    Pollard(ForestState& state, int max_nodes) : Accumulator(state)
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

    const Accumulator::BatchProof Prove(const std::vector<uint64_t>& targets) const
    {
        // TODO: prove does not really make sense for the pollard.
        // although you might want to prove cached leaves.
        const BatchProof proof(targets, std::vector<uint256>());
        return proof;
    }
};

#endif // UTREEXO_POLLARD_H
