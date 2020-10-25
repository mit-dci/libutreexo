#ifndef UTREEXO_ACCUMULATOR_H
#define UTREEXO_ACCUMULATOR_H

#include <memory>
#include <state.h>
#include <uint256.h>
#include <vector>

/**
 * Accumulator is an abstract class for a dynamic hash based accumulator.
 */
class Accumulator
{
protected:
    ForestState& state;

    class Node
    {
    protected:
        const ForestState& forestState;

        Node(const ForestState& state,
             uint64_t position)
            : forestState(state), position(position) {}

    public:
        const uint64_t position;
        virtual ~Node() {}

        virtual const uint256& hash() const = 0;
    };

    // Return the roots of the forest.
    virtual std::vector<std::shared_ptr<Accumulator::Node>> roots() const = 0;

    /*
     * Swap two subtrees in the forest.
     * Return the nodes that need to be rehashed.
     */
    virtual std::vector<std::shared_ptr<Accumulator::Node>> swapSubTrees(uint64_t posA, uint64_t posB) = 0;

    /*
     * mergeRoot and newLeaf only have the desired effect if called correctly.
     * newLeaf should be called to allocate a new leaf.
     * After calling newLeaf, mergeRoot should be called for every consecutive least significant bit that is set to 1.
     */

    // Return the result of the latest merge.
    virtual std::shared_ptr<Accumulator::Node> mergeRoot(uint64_t parentPos, uint256 parentHash) = 0;
    virtual std::shared_ptr<Accumulator::Node> newLeaf(uint256 hash) = 0;

    void printRoots(std::vector<std::shared_ptr<Accumulator::Node>>& roots);

public:
    class Leaf
    {
    public:
        virtual uint256 hash() const = 0;
        virtual bool remember() const = 0;

        virtual ~Leaf() {}
    };

    Accumulator(ForestState& state) : state(state) {}
    virtual ~Accumulator() {}

    void add(const std::vector<std::shared_ptr<Accumulator::Leaf>> leaves);
};


#endif // UTREEXO_ACCUMULATOR_H
