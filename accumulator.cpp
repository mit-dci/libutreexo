#include <accumulator.h>
#include <crypto/sha256.h>
#include <iostream>
#include <stdio.h>
#include <uint256.h>

uint256 parentHash(const uint256& left, const uint256& right)
{
    CSHA256 hasher;

    // copy the two hashes into one 64 byte buffer
    uint8_t data[64];
    memcpy(data, left.begin(), 32);
    memcpy(data + 32, right.begin(), 32);
    hasher.Write(data, 64);

    // finalize the hash and write it into parentHash
    uint256 parentHash;
    hasher.Finalize(parentHash.begin());

    return parentHash;
}

void Accumulator::printRoots(std::vector<std::shared_ptr<Accumulator::Node>>& roots)
{
    for (auto root : roots) {
        std::cout << "root: " << root->position << ":" << root->hash().GetHex() << std::endl;
    }
}

void Accumulator::add(const std::vector<std::shared_ptr<Accumulator::Leaf>> leaves)
{
    for (auto leaf = leaves.begin(); leaf < leaves.end(); leaf++) {
        std::vector<std::shared_ptr<Accumulator::Node>> roots = this->roots();

        auto root = roots.rbegin();
        std::shared_ptr<Accumulator::Node> newRoot = this->newLeaf((*leaf)->hash());
        for (uint8_t row = 0; this->state.hasRoot(row); row++) {
            uint64_t parentPos = this->state.parent(newRoot->position);
            uint256 hash = parentHash((*root)->hash(), newRoot->hash());
            newRoot = this->mergeRoot(parentPos, hash);
            root++;
        }
        this->state.add(1);
    }

    std::cout << "roots after adding:" << std::endl;
    std::vector<std::shared_ptr<Accumulator::Node>> roots = this->roots();
    printRoots(roots);
}
