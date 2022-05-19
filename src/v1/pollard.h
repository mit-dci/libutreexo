#ifndef UTREEXO_POLLARD_H
#define UTREEXO_POLLARD_H

#include "interface.h"

namespace utreexo {

using Hash = std::array<uint8_t, 32>;

class Pollard : public V1Accumulator
{
private:
    class InternalNode;
    using InternalSiblings = std::tuple<NodePtr<Pollard::InternalNode>, NodePtr<Pollard::InternalNode>>;

    /* Pollards implementation of Accumulator::Node */
    class Node;

    NodePtr<Pollard::InternalNode> m_remember;

    /*
     * Return the node and its sibling. Point path to the parent of the node.
     * The path to the node can be traversed in reverse order using the
     * Accumulator::Node::Parent function.
     */
    InternalSiblings ReadSiblings(uint64_t pos, NodePtr<V1Accumulator::Node>& path, bool record_path) const;
    InternalSiblings ReadSiblings(uint64_t pos) const;

    std::optional<const Hash> Read(uint64_t pos) const override;
    std::vector<Hash> ReadLeafRange(uint64_t pos, uint64_t range) const override;
    NodePtr<V1Accumulator::Node> SwapSubTrees(uint64_t from, uint64_t to) override;
    NodePtr<V1Accumulator::Node> MergeRoot(uint64_t parent_pos, Hash parent_hash) override;
    NodePtr<V1Accumulator::Node> NewLeaf(const std::pair<Hash, bool>& leaf) override;
    void FinalizeRemove(uint64_t next_num_leaves) override;

    void InitChildrenOfComputed(NodePtr<Node>& node,
                                NodePtr<Node>& left_child,
                                NodePtr<Node>& right_child,
                                bool& recover_left,
                                bool& recover_right);

    bool CreateProofTree(std::vector<NodePtr<Node>>& proof_tree,
                         std::vector<std::pair<NodePtr<Node>, int>>& recovery,
                         const BatchProof<Hash>& proof);

    bool VerifyProofTree(std::vector<NodePtr<Pollard::Node>> proof_tree,
                         const std::vector<Hash>& target_hashes,
                         const std::vector<Hash>& proof_hashes);

public:
    Pollard(const std::vector<Hash>& roots, uint64_t num_leaves);
    Pollard(uint64_t num_leaves);
    ~Pollard();

    bool Verify(const BatchProof<Hash>& proof, const std::vector<Hash>& target_hashes) override;

    /** Prune everything except the roots. */
    void Prune();

    uint64_t NumCachedLeaves() const { return m_remember ? m_remember.use_count() - 1 : 0; }
    uint64_t CountNodes(const NodePtr<Pollard::InternalNode>& node) const;
    uint64_t CountNodes() const;
};

};     // namespace utreexo
#endif // UTREEXO_POLLARD_H
