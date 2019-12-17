#ifndef UTREEXO_INT_TYPES_H
#define UTREEXO_INT_TYPES_H

#include <crypto/sha256.h>

#include <vector>

struct LeafTXO {
    // LeafTXO 's have a hash and a expiry date (block when that utxo gets used)
    CSHA256 Hash;
    int32_t Duration;
    bool Remember; // this leaf will be deleted soon, remember it
};

struct Proof {
    uint64_t Position; // where at the bottom of the tree it sits
    CSHA256 Payload; // hash of the thing itself (what's getting proved)
    std::vector<CSHA256> Siblings; // slice of siblings up to a root
};

struct arrow {
    // an arror describes the movement of a node from one position to another
    uint64_t from;
    uint64_t to;
};

struct arrowh {
    uint64_t from;
    uint64_t to;
    uint8_t ht;
};

struct Node {
    uint64_t Pos;
    CSHA256 Hash;
};

#endif //UTREEXO_INT_TYPES_H
