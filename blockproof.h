#ifndef UTREEXO_BLOCKPROOF_H
#define UTREEXO_BLOCKPROOF_H

#include <uint256.h>

#include <string>
#include <vector>
#include <unordered_map>

class BlockProof {
    // list of leaf locations to delete, along with a bunch of hashes that give the proof.
    // the position of the hashes is implied / computable from the leaf positions
    std::string ToString() const;
    std::vector<uint8_t> ToBytes() const;
    std::unordered_map<uint64_t, uint256> Reconstruct(uint64_t numleaves, uint8_t forestHeight);
private:
    uint64_t Targets;
    uint256 Proof;
};

#endif // UTREEXO_BLOCKPROOF_H
