#ifndef UTREEXO_POLNODE_H
#define UTREEXO_POLNODE_H

#include <uint256.h>

class polNode
{
public:
    uint256 auntOp();
    bool auntable();
    bool deadEnd();
    void chop();
    void prune();
    void leafPrune();
private:
    uint256 data;
    polNode* niece[2];
};

#endif // UTREEXO_POLNODE_H
