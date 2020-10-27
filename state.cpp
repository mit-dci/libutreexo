#include <algorithm>
#include <bitset>
#include <iostream>
#include <memory>
#include <state.h>

void print_vector(const std::vector<uint64_t>& vec)
{
    for (auto i : vec)
        std::cout << i << ' ';
    std::cout << std::endl;
}

void print_swaps(const std::vector<ForestState::Swap>& vec)
{
    for (auto i : vec)
        std::cout << i.collapse << " (" << i.from << ", " << i.to << ") ";
    std::cout << std::endl;
}

// Return the number of trailing one bits in n.
uint8_t trailingOnes(uint64_t n)
{
    uint64_t b = ~n & (n + 1);
    b--;
    b = (b & 0x5555555555555555) +
        ((b >> 1) & 0x5555555555555555);
    b = (b & 0x3333333333333333) +
        ((b >> 2) & 0x3333333333333333);
    b = (b & 0x0f0f0f0f0f0f0f0f) +
        ((b >> 4) & 0x0f0f0f0f0f0f0f0f);
    b = (b & 0x00ff00ff00ff00ff) +
        ((b >> 8) & 0x00ff00ff00ff00ff);
    b = (b & 0x0000ffff0000ffff) +
        ((b >> 16) & 0x0000ffff0000ffff);
    b = (b & 0x00000000ffffffff) +
        ((b >> 32) & 0x00000000ffffffff);

    return b;
}

uint8_t trailingZeros(uint64_t n)
{
    return trailingOnes(~n);
}

// TODO: Maybe export these?
uint64_t _rootPosition(uint8_t row, uint64_t numLeaves, uint8_t rows);
uint8_t _numRows(uint64_t numLeaves);
uint8_t _hasRoot(uint64_t numLeaves);
uint64_t _maxNodes(uint64_t numLeaves);

// positions

uint64_t ForestState::parent(uint64_t pos) const
{
    return (pos >> 1) | (1 << this->numRows());
}

uint64_t ForestState::ancestor(uint64_t pos, uint8_t rise) const
{
    if (rise == 0) {
        return pos;
    }

    uint8_t rows = this->numRows();
    uint64_t mask = this->maxNodes();
    return (pos >> rise | (mask << (rows - (rise - 1)))) & mask;
}

uint64_t ForestState::leftChild(uint64_t pos) const
{
    return (pos << 1) & (this->maxNodes());
}

uint64_t ForestState::child(uint64_t pos, uint64_t placement) const
{
    return this->leftChild(pos) | placement;
}

uint64_t ForestState::leftDescendant(uint64_t pos, uint8_t drop) const
{
    if (drop == 0) {
        return pos;
    }

    uint64_t mask = this->maxNodes();
    return (pos << drop) & mask;
}

uint64_t ForestState::cousin(uint64_t pos) const { return pos ^ 2; }

uint64_t ForestState::rightSibling(uint64_t pos) const { return pos | 1; }

uint64_t ForestState::sibling(uint64_t pos) const { return pos ^ 1; }

std::tuple<uint8_t, uint8_t, uint64_t> ForestState::path(uint64_t pos) const
{
    uint8_t rows = this->numRows();
    uint8_t row = this->detectRow(pos);

    // This is a bit of an ugly predicate.  The goal is to detect if we've
    // gone past the node we're looking for by inspecting progressively shorter
    // trees; once we have, the loop is over.

    // The predicate breaks down into 3 main terms:
    // A: pos << nh
    // B: mask
    // C: 1<<th & numleaves (treeSize)
    // The predicate is then if (A&B >= C)
    // A is position up-shifted by the row of the node we're targeting.
    // B is the "mask" we use in other functions; a bunch of 0s at the MSB side
    // and then a bunch of 1s on the LSB side, such that we can use bitwise AND
    // to discard high bits.  Together, A&B is shifting position up by nh bits,
    // and then discarding (zeroing out) the high bits.  This is the same as in
    // childMany.  C checks for whether a tree exists at the current tree
    // rows.  If there is no tree at th, C is 0.  If there is a tree, it will
    // return a power of 2: the base size of that tree.
    // The C term actually is used 3 times here, which is ugly; it's redefined
    // right on the next line.
    // In total, what this loop does is to take a node position, and
    // see if it's in the next largest tree.  If not, then subtract everything
    // covered by that tree from the position, and proceed to the next tree,
    // skipping trees that don't exist.

    uint8_t biggerTrees = 0;
    for (; ((pos << row) & ((2 << rows) - 1)) >= ((1 << rows) & this->numLeaves);
         rows--) {
        uint64_t treeSize = (1 << rows) & this->numLeaves;
        if (treeSize != 0) {
            pos -= treeSize;
            biggerTrees++;
        }
    }

    return std::make_tuple(biggerTrees, rows - row, ~pos);
}

std::pair<std::vector<uint64_t>, std::vector<uint64_t>>
ForestState::proofPositions(const std::vector<uint64_t>& targets) const
{
    uint64_t rows = this->numRows();

    // store for the proof and computed positions
    // proof positions are needed to verify,
    // computed positions are the positions of the targets as well as
    // positions that are computed while verifying.
    std::vector<uint64_t> proof, computed;

    std::vector<uint64_t>::const_iterator start = targets.cbegin(),
                                          end = targets.cend();

    // saves the reference to nextTargets in the loop from being destroyed
    std::shared_ptr<std::vector<uint64_t>> savior;

    for (uint8_t row = 0; row < rows; row++) {
        computed.insert(computed.end(), start, end);

        if (this->hasRoot(row) && start < end &&
            *(end - 1) == this->rootPosition(row)) {
            // remove roots from targets
            end--;
        }

        std::shared_ptr<std::vector<uint64_t>> nextTargets(
            new std::vector<uint64_t>());

        while (start < end) {
            int size = end - start;

            // look at the first 4 targets
            if (size > 3 && this->cousin(this->rightSibling(start[0])) ==
                                this->rightSibling(start[3])) {
                // the first and fourth target are cousins
                // => target 2 and 3 are also targets, both parents are targets of next
                // row
                nextTargets->insert(nextTargets->end(),
                                    {this->parent(start[0]), this->parent(start[3])});
                start += 4;
                continue;
            }

            // look at the first 3 targets
            if (size > 2 && this->cousin(this->rightSibling(start[0])) ==
                                this->rightSibling(start[2])) {
                // the first and third target are cousins
                // => the second target is either the sibling of the first
                // OR the sibiling of the third
                // => only the sibling that is not a target is appended to the proof positions
                if (this->rightSibling(start[1]) == this->rightSibling(start[0])) {
                    proof.push_back(this->sibling(start[2]));
                } else {
                    proof.push_back(this->sibling(start[0]));
                }

                nextTargets->insert(nextTargets->end(),
                                    {this->parent(start[0]), this->parent(start[2])});
                start += 3;
                continue;
            }

            // look at the first 2 targets
            if (size > 1) {
                if (this->rightSibling(start[0]) == start[1]) {
                    // the first and the second target are siblings
                    // => parent is a target for the next.
                    nextTargets->push_back(this->parent(start[0]));
                    start += 2;
                    continue;
                }

                if (this->cousin(this->rightSibling(start[0])) ==
                    this->rightSibling(start[1])) {
                    // the first and the second target are cousins
                    // => both siblings are part of the proof
                    // => both parents are targets for the next row
                    proof.insert(proof.end(),
                                 {this->sibling(start[0]), this->sibling(start[1])});
                    nextTargets->insert(nextTargets->end(),
                                        {this->parent(start[0]), this->parent(start[1])});
                    start += 2;
                    continue;
                }
            }

            // look at the first target
            proof.push_back(this->sibling(start[0]));
            nextTargets->push_back(this->parent(start[0]));
            start++;
        }

        start = nextTargets->cbegin();
        end = nextTargets->cend();
        savior = nextTargets;
    }

    return std::make_pair(proof, computed);
}

// roots

uint8_t ForestState::numRoots() const
{
    std::bitset<64> bits(this->numLeaves);
    return bits.count();
}

bool _hasRoot(uint64_t numLeaves, uint8_t row)
{
    return (numLeaves >> row) & 1;
}

bool ForestState::hasRoot(uint8_t row) const
{
    return _hasRoot(this->numLeaves, row);
}

uint64_t _rootPosition(uint8_t row, uint64_t numLeaves, uint8_t rows)
{
    uint64_t mask = _maxNodes(numLeaves);
    uint64_t before = numLeaves & (mask << (row + 1));
    uint64_t shifted = (before >> row) | (mask << (rows - (row - 1)));
    return shifted & mask;
}

uint64_t ForestState::rootPosition(uint8_t row) const
{
    return _rootPosition(row, this->numLeaves, this->numRows());
}

std::vector<uint64_t> ForestState::rootPositions() const
{
    std::vector<uint64_t> roots;
    for (uint8_t row = this->numRows(); row >= 0 && row < 64; row--) {
        if (this->hasRoot(row)) {
            roots.push_back(this->rootPosition(row));
        }
    }
    return roots;
}

std::vector<uint64_t> ForestState::rootPositions(uint64_t numLeaves) const
{
    std::vector<uint64_t> roots;
    for (uint8_t row = this->numRows(); row >= 0 && row < 64; row--) {
        if (_hasRoot(numLeaves, row)) {
            roots.push_back(_rootPosition(row, numLeaves, this->numRows()));
        }
    }
    return roots;
}
// rows


uint8_t _numRows(uint64_t numLeaves)
{
    // numRows works by:
    // 1. Find the next power of 2 from the given n leaves.
    // 2. Calculate the log2 of the result from step 1.
    //
    // For example, if the given number is 9, the next power of 2 is
    // 16. This log2 of this number is how many rows there are in the
    // given tree.
    //
    // This works because while Utreexo is a collection of perfect
    // trees, the allocated number of leaves is always a power of 2.
    // For Utreexo trees that don't have leaves that are power of 2,
    // the extra space is just unallocated/filled with zeros.

    // Find the next power of 2

    uint64_t n = numLeaves;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    n++;

    // log of 2 is the tree depth/height
    // if n == 0, there will be 64 traling zeros but actually no tree rows.
    // we clear the 6th bit to return 0 in that case.
    return uint8_t(trailingZeros(n) & ~int(64));
}


uint8_t ForestState::numRows() const
{
    return _numRows(this->numLeaves);
}

uint8_t ForestState::detectRow(uint64_t pos) const
{
    uint64_t marker = 1 << this->numRows();
    uint8_t row = 0;

    for (; (pos & marker) != 0; row++) {
        marker >>= 1;
    }

    return row;
}

uint8_t ForestState::rowOffset(uint64_t pos) const
{
    uint8_t row = this->detectRow(pos);
    uint64_t marker = this->maxNodes();
    return (0xFFFFFFFFFFFFFFFF << (this->numRows() + 1 - row)) & marker;
}

// transform

void ForestState::add(uint64_t num) { this->numLeaves += num; }

void ForestState::remove(uint64_t num) { this->numLeaves -= num; }

std::vector<std::vector<ForestState::Swap>>
ForestState::transform(const std::vector<uint64_t>& targets) const
{
    uint8_t rows = this->numRows();
    uint64_t nextNumLeaves = this->numLeaves - targets.size();

    std::vector<std::vector<ForestState::Swap>> swaps;
    std::vector<ForestState::Swap> collapses;
    swaps.reserve(rows);
    collapses.reserve(rows);

    std::vector<uint64_t> currentRowTargets(targets);

    for (uint8_t row = 0; row < rows && currentRowTargets.size() > 0; row++) {
        bool rootPresent = this->hasRoot(row);
        uint64_t rootPos = this->rootPosition(row);

        if (rootPresent && *(currentRowTargets.end() - 1) == rootPos) {
            currentRowTargets.pop_back();
            rootPresent = false;
        }

        bool deletionRemains = currentRowTargets.size() % 2 != 0;

        //extractPair.first are the parents of the siblings, extractPair.second is the input with out siblings.
        std::pair<std::vector<uint64_t>, std::vector<uint64_t>> extractPair =
            computeNextRowTargets(currentRowTargets, deletionRemains, rootPresent);
        swaps.push_back(this->makeSwaps(extractPair.second, deletionRemains, rootPresent, rootPos));
        collapses.push_back(this->makeCollapse(extractPair.second, deletionRemains, rootPresent, row, nextNumLeaves));

        currentRowTargets = extractPair.first;
    }

    // Convert collapses to swaps and append them to the swaps list.
    this->convertCollapses(swaps, collapses);

    return swaps;
}

// misc

uint64_t _maxNodes(uint64_t numLeaves) { return (2 << _numRows(numLeaves)) - 1; }
uint64_t ForestState::maxNodes() const { return _maxNodes(numLeaves); }

// private

std::pair<std::vector<uint64_t>, std::vector<uint64_t>>
ForestState::computeNextRowTargets(const std::vector<uint64_t>& targets,
                                   bool deletionRemains,
                                   bool rootPresent) const
{
    std::vector<uint64_t> targetsWithoutSiblings, parents;

    std::vector<uint64_t>::const_iterator start = targets.begin();
    while (start < targets.end()) {
        if (start < targets.end() - 1 && this->rightSibling(start[0]) == start[1]) {
            // These two targets are siblings. In the context of computing swaps there is no need to swap them,
            // but we might want to swap their parent in the next row.
            // => store the parent.
            parents.push_back(this->parent(start[0]));
            start += 2;
            continue;
        }

        // This target has no sibling.
        targetsWithoutSiblings.push_back(start[0]);
        if (targetsWithoutSiblings.size() % 2 == 0) {
            parents.push_back(this->parent(start[0]));
        }
        start++;
    }

    if (deletionRemains && !rootPresent) {
        parents.push_back(this->parent(targetsWithoutSiblings.back()));
    }

    return std::make_pair(parents, targetsWithoutSiblings);
}

std::vector<ForestState::Swap> ForestState::makeSwaps(const std::vector<uint64_t>& targets,
                                                      bool deletionRemains,
                                                      bool rootPresent,
                                                      uint64_t rootPos) const
{
    // +1 for deletionRemains && rootPresent == true
    uint32_t numSwaps = (targets.size() >> 1) + 1;
    std::vector<ForestState::Swap> swaps;
    swaps.reserve(numSwaps);

    std::vector<uint64_t>::const_iterator start = targets.begin();
    while (targets.end() - start > 1) {
        // Look at 2 targets at a time and create a swap that turns both deletions into siblings.
        // This is possible because all nodes in `targets` are not siblings (thanks to `computeNextRowTargets`).
        swaps.push_back(ForestState::Swap(this->sibling(start[1]), start[0]));
        start += 2;
    }

    if (deletionRemains && rootPresent) {
        // there is a remaining deletion and a root on this row
        // => swap target with the root.
        swaps.push_back(ForestState::Swap(rootPos, start[0]));
    }

    return swaps;
}

ForestState::Swap ForestState::makeCollapse(const std::vector<uint64_t>& targets,
                                            bool deletionRemains,
                                            bool rootPresent,
                                            uint8_t row,
                                            uint64_t nextNumLeaves) const
{
    // The position of the root on this row after the deletion.
    uint64_t rootDest = _rootPosition(row, nextNumLeaves, this->numRows());

    if (!deletionRemains && rootPresent) {
        // No deletion remaining but there is a root.
        // => Collapse the root to its position after the deletion.
        return ForestState::Swap(this->rootPosition(row), rootDest, true);
    }

    if (deletionRemains && !rootPresent) {
        // There is no root but there is a remaining deletion.
        // => The sibling of the remaining deletion becomes a root.
        return ForestState::Swap(this->sibling(targets.back()), rootDest, true);
    }

    // No collapse on this row.
    // This will be ignored in `convertCollapses` because collapse=false.
    return ForestState::Swap(0, 0);
}

void ForestState::convertCollapses(std::vector<std::vector<ForestState::Swap>>& swaps,
                                   std::vector<ForestState::Swap>& collapses) const
{
    if (collapses.size() == 0) {
        // If there is nothing to collapse, we're done
        return;
    }


    for (uint8_t row = collapses.size() - 1; row != 0; row--) {
        for (ForestState::Swap swap : swaps.at(row)) {
            // For every swap in the row, convert the collapses below the swap.
            this->swapInRow(swap, collapses, row);
        }

        if (!collapses.at(row).collapse) {
            // There is no collapse in this row.
            continue;
        }

        // For the collapse on this row, convert the other collapses located below.
        this->swapInRow(collapses.at(row), collapses, row);
    }

    // Append collapses to swaps.
    uint8_t row = 0;
    for (ForestState::Swap collapse : collapses) {
        if (collapse.from != collapse.to && collapse.collapse) {
            swaps.at(row).push_back(collapse);
        }
        row++;
    }
}

void ForestState::swapInRow(ForestState::Swap swap,
                            std::vector<ForestState::Swap>& collapses,
                            uint8_t swapRow) const
{
    for (uint8_t collapseRow = 0; collapseRow < swapRow; collapseRow++) {
        if (!collapses.at(collapseRow).collapse) {
            continue;
        }
        this->swapIfDescendant(swap, collapses.at(collapseRow), swapRow, collapseRow);
    }
}

void ForestState::swapIfDescendant(ForestState::Swap swap,
                                   ForestState::Swap& collapse,
                                   uint8_t swapRow,
                                   uint8_t collapseRow) const
{
    uint8_t rowDiff = swapRow - collapseRow;
    uint64_t ancestor = this->ancestor(collapse.to, rowDiff);
    if ((ancestor == swap.from) != (ancestor == swap.to)) {
        collapse.to ^= (swap.from ^ swap.to) << rowDiff;
    }
}
