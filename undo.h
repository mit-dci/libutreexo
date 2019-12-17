#ifndef UTREEXO_UNDO_H
#define UTREEXO_UNDO_H

#include <string>
#include <vector>

class uint256;

class undoBlock {
public:
    std::string ToString() const;
    // blockUndo is all the data needed to undo a block: number of adds,
    // and all the hashes that got deleted and where they were from
    uint32_t numAdds; // number of adds in the block
    std::vector<uint64_t> positions; // position of all deletions this block
    std::vector<uint256> hashes; // hashes that were deleted
};

#endif // UTREEXO_UNDO_H
