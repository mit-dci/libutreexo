#include <crypto/sha256.h>
#include <iostream>
#include <pollard.h>
#include <string.h>

// Pollard

std::vector<std::shared_ptr<Accumulator::Node>> Pollard::roots() const
{
    std::vector<uint64_t> vRoots = this->state.rootPositions();
    auto rootPosIt = vRoots.begin();
    std::vector<std::shared_ptr<Accumulator::Node>> forestRoots;
    for (std::shared_ptr<InternalNode> root : this->mRoots) {
        forestRoots.push_back(std::shared_ptr<Accumulator::Node>(
            (Accumulator::Node*)(new Pollard::Node(this->state, *rootPosIt, root))));
        rootPosIt++;
    }

    return forestRoots;
}

std::vector<std::shared_ptr<Accumulator::Node>> Pollard::swapSubTrees(uint64_t posA, uint64_t posB)
{
    // TODO: implement
    return std::vector<std::shared_ptr<Accumulator::Node>>();
}

std::shared_ptr<Accumulator::Node> Pollard::newLeaf(uint256 hash)
{
    auto intNode = std::shared_ptr<InternalNode>(new InternalNode(hash));
    this->mRoots.push_back(intNode);

    return std::shared_ptr<Accumulator::Node>(
        (Accumulator::Node*)new Pollard::Node(this->state, this->state.numLeaves, intNode));
}

std::shared_ptr<Accumulator::Node> Pollard::mergeRoot(uint64_t parentPos, uint256 parentHash)
{
    std::shared_ptr<InternalNode> intLeft = this->mRoots.back();
    this->mRoots.pop_back();
    std::shared_ptr<InternalNode> intRight = this->mRoots.back();
    this->mRoots.pop_back();

    // swap nieces
    std::swap(intLeft->nieces, intRight->nieces);

    // create internal node
    auto intNode = std::shared_ptr<InternalNode>(new InternalNode(parentHash));
    intNode->nieces[0] = intLeft;
    intNode->nieces[1] = intRight;
    intNode->prune();

    this->mRoots.push_back(intNode);

    return std::shared_ptr<Accumulator::Node>((Accumulator::Node*)(new Pollard::Node(this->state, parentPos, intNode)));
}

// Pollard::Node

const uint256& Pollard::Node::hash() const
{
    return this->node->hash;
}

// Pollard::InternalNode

void Pollard::InternalNode::prune()
{
    if (this->nieces[0] || this->nieces[0]->deadEnd()) {
        this->nieces[0] = nullptr;
    }

    if (this->nieces[1] || this->nieces[1]->deadEnd()) {
        this->nieces[1] = nullptr;
    }
}

bool Pollard::InternalNode::deadEnd()
{
    return !this->nieces[0] && !this->nieces[1];
}

Pollard::InternalNode::~InternalNode() {}
