#ifndef UTREEXO_NODE_H
#define UTREEXO_NODE_H

#include <accumulator.h>
#include <array>
#include <nodepool.h>

namespace utreexo {

using Hash = std::array<uint8_t, 32>;

class Accumulator::Node
{
public:
    // The number of leaves at the time this node was created.
    uint64_t m_num_leaves;

    // A pointer to the parent node.
    // This is useful if you want to rehash a path from the bottom up.
    NodePtr<Accumulator::Node> m_parent;

    // The position of the node in the forest.
    uint64_t m_position;

    virtual ~Node() {}

    /*
     * Return the hash of the node.
     * This does not compute the hash only returns a previously computed hash.
     */
    virtual const Hash& GetHash() const = 0;

    /* Recompute the hash from children nodes and return it. */
    virtual void ReHash() = 0;

    /*
     * Return the parent of the node.
     * A return value of nullptr does *not* always indicate that a tree top was reached. 
     */
    virtual NodePtr<Accumulator::Node> Parent() const { return m_parent; }

    virtual void NodePoolDestroy() { m_parent = nullptr; }
};

}; // namespace utreexo

#endif // UTREEXO_NODE_H
