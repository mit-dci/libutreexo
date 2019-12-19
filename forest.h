#ifndef UTREEXO_FOREST_H
#define UTREEXO_FOREST_H

#include <uint256.h>

#include <string>
#include <unordered_map>
#include <vector>

class undoBlock;
class BlockProof;

struct LeafTXO;
struct Proof;

class Forest
{
public:
    void Add(const std::vector<LeafTXO> adds);
    void Remove(const std::vector<uint64_t>& dels);
    undoBlock Modify(const std::vector<LeafTXO>& adds, const std::vector<uint64_t> dels);

    Proof Prove(const uint256& wanted) const;
    std::vector<Proof> ProveMany(const std::vector<uint256>& wanted) const;
    bool Verify(const Proof& p) const;
    bool VerifyMany(const std::vector<Proof>& p) const;
    BlockProof ProveBlock(const std::vector<uint256>& hs) const;
    bool VerifyBlockProof(const BlockProof& bp) const;

    void Undo(const undoBlock& ub) const;
    void PosMapSanity() const;
    std::string Stats() const;
    std::string ToString() const;
    std::vector<uint256> GetTops() const;
    undoBlock BuildUndoData(uint64_t numadds, const std::vector<uint64_t>& dels) const;

    uint64_t HistoricHashes;
private:
    void reMap(uint8_t destHeight);
    void reHash(const std::vector<uint64_t>& dirt);
    void cleanup();

    void sanity() const;
    uint64_t numLeaves;
    uint8_t height;
    std::vector<uint256> forest;
    std::unordered_map<uint64_t, uint256> positionMap;
    std::unordered_map<uint64_t, bool> dirtyMap;
};

#endif // UTREEXO_FOREST_H
