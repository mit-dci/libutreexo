#ifndef UTREEXO_NODE_H
#define UTREEXO_NODE_H

#include "interface.h"
#include <array>

namespace utreexo {

using Hash = std::array<uint8_t, 32>;

class V1Accumulator::Node
{
public:
    // The number of leaves at the time this node was created.
    uint64_t m_num_leaves;

    // A pointer to the parent node.
    // This is useful if you want to rehash a path from the bottom up.
    NodePtr<V1Accumulator::Node> m_parent;

    // The position of the node in the forest.
    uint64_t m_position;

    virtual ~Node()
    {
        m_parent = nullptr;
    }

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
    virtual NodePtr<V1Accumulator::Node> Parent() const { return m_parent; }
};

}; // namespace utreexo

#endif // UTREEXO_NODE_H
