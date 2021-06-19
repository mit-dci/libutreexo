
#ifndef UTREEXO_STATE_H
#define UTREEXO_STATE_H

#include <stdint.h>
#include <vector>

namespace utreexo {

/**
 * A wrapper around the number of leaves the accumulator forest
 * that provides utility functions to compute positions and swaps,
 * check for roots, etc.
 */
class ForestState
{
public:
    /**
     * Swap represents a swap between two nodes in the forest.
     */
    class Swap
    {
    public:
        // Swap from <-> to
        uint64_t m_from, m_to;
        // Does this swap resemble a collapse.
        // See `makeCollapse` for collapse definition.
        bool m_collapse;

        bool m_is_range_swap;
        uint64_t m_range;

        explicit Swap(uint64_t from, uint64_t to)
            : m_from(from), m_to(to), m_collapse(false), m_is_range_swap(false), m_range(0) {}
        explicit Swap(uint64_t from, uint64_t to, bool collapse)
            : m_from(from), m_to(to), m_collapse(collapse), m_is_range_swap(false), m_range(0) {}
        explicit Swap(uint64_t from, uint64_t to, uint64_t range)
            : m_from(from), m_to(to), m_collapse(false), m_is_range_swap(true), m_range(range) {}

        Swap ToLeaves(ForestState state) const;
    };

    // The number of leaves in the forest.
    // TODO: make this private.
    uint64_t m_num_leaves;

    ForestState() : m_num_leaves(0) {}
    ForestState(uint64_t n) : m_num_leaves(n) {}

    // Functions to compute positions:

    // Return the parent positon.
    // Same as ancestor(pos, 1)
    uint64_t Parent(uint64_t pos) const;
    uint64_t Ancestor(uint64_t pos, uint8_t rise) const;
    // Return the position of the left child.
    // Same as leftDescendant(pos, 1).
    uint64_t LeftChild(uint64_t pos) const;
    uint64_t Child(uint64_t pos, uint64_t placement) const;
    uint64_t LeftDescendant(uint64_t pos, uint8_t drop) const;
    // Return the position of the cousin.
    // Placement (left,right) remains.
    uint64_t Cousin(uint64_t pos) const;
    // Return the position of the right sibling.
    // A right position is its own right sibling.
    uint64_t RightSibling(uint64_t pos) const;
    // Return the position of the sibling.
    uint64_t Sibling(uint64_t pos) const;

    /**
     * Compute the path to the position.
     * Return the index of the tree the position is in, the distance from the node
     * to its root and the bitfield indicating the path.
     */
    std::tuple<uint8_t, uint8_t, uint64_t> Path(uint64_t pos) const;

    /**
     * Compute the proof positions needed to proof the existence of some targets.
     * Return the proof positions and the positions of the nodes that are computed
     * when verifying a proof.
     */
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>>
    ProofPositions(const std::vector<uint64_t>& targets) const;

    // Functions for root stuff:

    // Return the number of roots.
    uint8_t NumRoots() const;
    // Check if there is a root on a row.
    bool HasRoot(uint8_t row) const;
    // Return the root position on a row.
    uint64_t RootPosition(uint8_t row) const;
    uint64_t RootPosition(uint8_t row, uint64_t num_leaves) const;
    // Return the positions of the roots in the forest
    std::vector<uint64_t> RootPositions() const;
    std::vector<uint64_t> RootPositions(uint64_t num_leaves) const;

    uint8_t RootIndex(uint64_t pos) const;

    // Functions for rows:

    // Return the number of rows.
    uint8_t NumRows() const;
    // Return the row of the position.
    uint8_t DetectRow(uint64_t pos) const;
    // Return the position of the first node in the row.
    uint64_t RowOffset(uint64_t pos) const;
    uint64_t RowOffset(uint8_t row) const;

    /**
     * Compute the remove transformation swaps.
     * Return a vector of swaps for every row in the forest (from bottom to top).
     */
    std::vector<std::vector<ForestState::Swap>>
    Transform(const std::vector<uint64_t>& targets) const;

    std::vector<ForestState::Swap> UndoTransform(const std::vector<uint64_t>& targets) const;

    // Misc:

    // Return the maximum number of nodes in the forest.
    uint64_t MaxNodes() const;

    bool CheckTargetsSanity(const std::vector<uint64_t>& targets) const;

private:
    /*
     * Return all targets of this row and all targets of the next row.
     * (targets are the nodes that will be deleted)
     */
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>>
    ComputeNextRowTargets(const std::vector<uint64_t>& targets,
                          bool deletion_remains,
                          bool root_present) const;

    /*
     * 
     */
    std::vector<ForestState::Swap> MakeSwaps(const std::vector<uint64_t>& targets,
                                             bool deletion_remains,
                                             bool root_present,
                                             uint64_t rootPos) const;

    /*
     * 
     */
    ForestState::Swap MakeCollapse(const std::vector<uint64_t>& targets,
                                   bool deletion_remains,
                                   bool root_present,
                                   uint8_t row,
                                   uint64_t next_num_leaves) const;

    /*
     * 
     */
    void ConvertCollapses(std::vector<std::vector<ForestState::Swap>>& swaps,
                          std::vector<ForestState::Swap>& collapses) const;

    void SwapInRow(ForestState::Swap swap,
                   std::vector<ForestState::Swap>& collapses,
                   uint8_t swapRow) const;

    void SwapIfDescendant(ForestState::Swap swap,
                          ForestState::Swap& collapse,
                          uint8_t swapRow,
                          uint8_t collapse_row) const;
};

// TODO: remove these
void print_vector(const std::vector<uint64_t>& vec);
void print_swaps(const std::vector<ForestState::Swap>& vec);

};     // namespace utreexo
#endif // UTREEXO_STATE_H
