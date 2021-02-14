#ifndef UTREEXO_POLLARD_H
#define UTREEXO_POLLARD_H

#include <accumulator.h>
#include <memory>
#include <nodepool.h>

namespace utreexo {

class Pollard : public Accumulator
{
private:
    class InternalNode;
    /* Pollards implementation of Accumulator::Node */
    class Node;

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
    Pollard(uint64_t num_leaves, int max_nodes);
    ~Pollard();

    bool Prove(BatchProof& proof, const std::vector<Hash>& target_hashes) const override;
};

};     // namespace utreexo
#endif // UTREEXO_POLLARD_H
