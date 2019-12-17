#ifndef UTREEXO_POLLARD_H
#define UTREEXO_POLLARD_H

#include <string>
#include <unordered_map>
#include <vector>

class polNode;
class uint256;
class Forest;
class BlockProof;

struct arrow;
struct LeafTXO;

class Pollard
{
public:
    void Modify(const std::vector<LeafTXO>& adds, const std::vector<uint64_t>& dels);
    std::string Stats() const;
    void add(const std::vector<LeafTXO>& adds);
    bool addOne(const uint256& add, bool remember);
    bool rem(const std::vector<uint64_t>& dels);
    void moveNode(const arrow& a);
    void reHashOne(uint64_t pos);

    Forest toFull();
    void IngestBlockProof(const BlockProof& bp);
    uint8_t height();
    std::vector<uint256> topHashesReverse();

private:
    using tops_iter = std::vector<polNode>::iterator;
    std::pair<std::vector<tops_iter>, std::vector<tops_iter>> descendToPos(uint64_t);
    uint64_t numLeaves; // number of leaves in the pollard forest
    std::vector<polNode> tops; // slice of the tree tops, which are polNodes.
    // tops are in big to small order
    // BUT THEY'RE WEIRD!  The left / right children are actual children,
    // not neices as they are in every lower level.
    uint64_t hashesEver;
    uint64_t rememberEver;
    uint64_t overWire;
};

#endif // UTREEXO_POLLARD_H

