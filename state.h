
#ifndef UTREEXO_STATE_H
#define UTREEXO_STATE_H

#include <stdint.h>
#include <vector>

/**
 * ForestState represents the state of the accumulator forest.
 * Provides utility functions to compute positions, check for roots, transforms, rows, ...
 */
class ForestState
{
public:
    /**
     * Swap represents a swap between two nodes in the forest.
     */
    struct Swap {
        // Swap from <-> to
        uint64_t from, to;
        // Does this swap resemble a collapse.
        // See `makeCollapse` for collapse definition.
        bool collapse;

        Swap(uint64_t from, uint64_t to)
            : from(from), to(to), collapse(false) {}
        Swap(uint64_t from, uint64_t to, bool collapse)
            : from(from), to(to), collapse(collapse) {}
    };

    // The number of leaves in the forest.
    uint64_t numLeaves;

    ForestState() : numLeaves(0) {}
    ForestState(uint64_t n) : numLeaves(n) {}

    // Functions to compute positions:

    // Return the parent positon.
    // Same as ancestor(pos, 1)
    uint64_t parent(uint64_t pos) const;
    uint64_t ancestor(uint64_t pos, uint8_t rise) const;
    // Return the position of the left child.
    // Same as leftDescendant(pos, 1).
    uint64_t leftChild(uint64_t pos) const;
    uint64_t leftDescendant(uint64_t pos, uint8_t drop) const;
    // Return the position of the cousin.
    // Placement (left,right) remains.
    uint64_t cousin(uint64_t pos) const;
    // Return the position of the right sibling.
    // A right position is its own right sibling.
    uint64_t rightSibling(uint64_t pos) const;
    // Return the position of the sibling.
    uint64_t sibling(uint64_t pos) const;

    /**
     * Compute the path to the position.
     * Return the index of the tree the position is in, the distance from the node
     * to its root and the bitfield indicating the path.
     */
    std::tuple<uint8_t, uint8_t, uint64_t> path(uint64_t pos) const;

    /**
     * Compute the proof positions needed to proof the existence of some targets.
     * Return the proof positions and the positions of the nodes that are computed
     * when verifying a proof.
     */
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>>
    proofPositions(const std::vector<uint64_t>& targets) const;

    // Functions for root stuff:

    // Return the number of roots.
    uint8_t numRoots() const;
    // Check if there is a root on a row.
    bool hasRoot(uint8_t row) const;
    // Return the root position on a row.
    uint64_t rootPosition(uint8_t row) const;
    uint64_t rootPosition(uint8_t row, uint64_t numLeaves) const;
    // Return the positions of the roots in the forest
    std::vector<uint64_t> rootPositions() const;

    // Functions for rows:

    // Return the number of rows.
    uint8_t numRows() const;
    // Return the row of the position.
    uint8_t detectRow(uint64_t pos) const;
    // Return the position of the first node in the row.
    uint8_t rowOffset(uint64_t pos) const;

    // Functions for modification of the state.
    // These functions are the only ones that may change the state.

    // Add leaves to the state.
    void add(uint64_t num);

    // Remove leaves from the state.
    void remove(uint64_t num);

    /**
     * Compute the remove transformation swaps.
     * Return a vector of swaps for every row in the forest (from bottom to top).
     */
    std::vector<std::vector<ForestState::Swap>>
    transform(const std::vector<uint64_t>& targets) const;

    // Misc:

    // Return the maximum number of nodes in the forest.
    uint64_t maxNodes() const;

private:
    /*
     * Return all targets of this row and all targets of the next row.
	 * (targets are the nodes that will be deleted)
     */
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>>
    computeNextRowTargets(const std::vector<uint64_t>& targets,
                          bool deletionRemains,
                          bool rootPresent) const;

    /*
     * 
     */
    std::vector<ForestState::Swap> makeSwaps(const std::vector<uint64_t>& targets,
                                             bool deletionRemains,
                                             bool rootPresent,
                                             uint64_t rootPos) const;

    /*
     * 
     */
    ForestState::Swap makeCollapse(const std::vector<uint64_t>& targets,
                                   bool deletionRemains,
                                   bool rootPresent,
                                   uint8_t row,
                                   uint64_t nextNumLeaves) const;

    /*
     * 
     */
    void convertCollapses(std::vector<std::vector<ForestState::Swap>>& swaps,
                          std::vector<ForestState::Swap>& collapses) const;

    void swapInRow(ForestState::Swap swap,
                   std::vector<ForestState::Swap>& collapses,
                   uint8_t swapRow) const;

    void swapIfDescendant(ForestState::Swap swap,
                          ForestState::Swap& collapse,
                          uint8_t swapRow,
                          uint8_t collapseRow) const;
};

void print_vector(const std::vector<uint64_t>& vec);
void print_swaps(const std::vector<ForestState::Swap>& vec);

#endif // UTREEXO_STATE_H