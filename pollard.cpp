#include <algorithm>
#include <crypto/sha256.h>
#include <iostream>
#include <pollard.h>
#include <string.h>

// Get the internal node from a std::shared_ptr<Accumulator::Node>.
#define INTERNAL_NODE(accNode) (((Pollard::Node*)accNode.get())->node)

// Pollard

std::vector<std::shared_ptr<Pollard::InternalNode>> Pollard::read(uint64_t pos, std::shared_ptr<Accumulator::Node>& reHashPath) const
{
    std::vector<std::shared_ptr<Pollard::InternalNode>> family;
    family.reserve(2);

    // Get the path to the position.
    uint8_t tree, pathLength;
    uint64_t pathBits;
    std::tie(tree, pathLength, pathBits) = this->state.path(pos);

    // There is no node above a root.
    reHashPath = nullptr;
    std::shared_ptr<Pollard::InternalNode> node = INTERNAL_NODE(this->mRoots[tree]),
                                           sibling = INTERNAL_NODE(this->mRoots[tree]);
    uint64_t nodePos = this->state.rootPositions()[tree];

    if (pathLength == 0) {
        family.push_back(node);
        family.push_back(sibling);
        return family;
    }


    // Traverse the pollard until the desired position is reached.
    for (uint8_t i = 0; i < pathLength; i++) {
        uint8_t lr = (pathBits >> (pathLength - 1 - i)) & 1;
        uint8_t lrSib = this->state.sibling(lr);

        auto intNode = new Pollard::Node(this->state, nodePos, node, reHashPath, sibling);
        reHashPath = std::shared_ptr<Accumulator::Node>((Accumulator::Node*)intNode);

        node = sibling->nieces[lrSib];
        sibling = sibling->nieces[lr];

        nodePos = this->state.child(nodePos, lrSib);
    }

    family.push_back(node);
    family.push_back(sibling);
    return family;
}

std::shared_ptr<Accumulator::Node> Pollard::swapSubTrees(uint64_t posA, uint64_t posB)
{
    // TODO: get rid of tmp.
    std::shared_ptr<Accumulator::Node> reHashPath, tmp;
    std::vector<std::shared_ptr<InternalNode>> familyA, familyB;
    familyA = this->read(posA, tmp);
    familyB = this->read(posB, reHashPath);

    std::shared_ptr<InternalNode> nodeA, siblingA,
        nodeB, siblingB;
    nodeA = familyA.at(0);
    siblingA = familyA.at(1);
    nodeB = familyB.at(0);
    siblingB = familyB.at(1);

    // Swap the hashes of node a and b.
    std::swap(nodeA->hash, nodeB->hash);
    // Swap the nieces of the siblings of a and b.
    std::swap(siblingA->nieces, siblingB->nieces);

    return reHashPath;
}

std::shared_ptr<Accumulator::Node> Pollard::newLeaf(uint256 hash)
{
    auto intNode = std::shared_ptr<InternalNode>(new InternalNode(hash));
    intNode->nieces[0] = intNode;

    this->mRoots.push_back(std::shared_ptr<Accumulator::Node>(
        (Accumulator::Node*)new Pollard::Node(this->state, this->state.numLeaves, intNode)));

    return this->mRoots.back();
}

std::shared_ptr<Accumulator::Node> Pollard::mergeRoot(uint64_t parentPos, uint256 parentHash)
{
    std::shared_ptr<InternalNode> intRight = INTERNAL_NODE(this->mRoots.back());
    this->mRoots.pop_back();
    std::shared_ptr<InternalNode> intLeft = INTERNAL_NODE(this->mRoots.back());
    this->mRoots.pop_back();

    // swap nieces
    std::swap(intLeft->nieces, intRight->nieces);

    // create internal node
    auto intNode = std::shared_ptr<InternalNode>(new InternalNode(parentHash));
    intNode->nieces[0] = intLeft;
    intNode->nieces[1] = intRight;
    intNode->prune();

    this->mRoots.push_back(std::shared_ptr<Accumulator::Node>(
        (Accumulator::Node*)(new Pollard::Node(this->state, parentPos, intNode))));

    return this->mRoots.back();
}

void Pollard::finalizeRemove(const ForestState nextState)
{
    // Compute the positions of the new roots in the current state.
    std::vector<uint64_t> newPositions = this->state.rootPositions(nextState.numLeaves);

    // Select the new roots.
    std::vector<std::shared_ptr<Accumulator::Node>> newRoots;
    newRoots.reserve(newPositions.size());

    for (uint64_t newPos : newPositions) {
        std::shared_ptr<Accumulator::Node> unusedPath = nullptr;
        std::vector<std::shared_ptr<InternalNode>> family = this->read(newPos, unusedPath);
        newRoots.push_back(std::shared_ptr<Accumulator::Node>(
            (Accumulator::Node*)new Pollard::Node(this->state, newPos, family.at(0))));
    }

    this->mRoots = newRoots;
}

// Pollard::Node

const uint256& Pollard::Node::hash() const
{
    return this->node->hash;
}

void Pollard::Node::reHash()
{
    if (!this->sibling->nieces[0] || !this->sibling->nieces[1]) {
        // TODO: error could not rehash one of the children is not known.
        // This will happen if there are duplicates in the dirtyNodes in Accumulator::remove.
        return;
    }

    this->node->hash = Accumulator::parentHash(this->sibling->nieces[0]->hash, this->sibling->nieces[1]->hash);
    this->sibling->prune();
}

// Pollard::InternalNode

void Pollard::InternalNode::prune()
{
    if (!this->nieces[0] || this->nieces[0]->deadEnd()) {
        this->nieces[0] = nullptr;
    }

    if (!this->nieces[1] || this->nieces[1]->deadEnd()) {
        this->nieces[1] = nullptr;
    }
}

bool Pollard::InternalNode::deadEnd()
{
    return !this->nieces[0] && !this->nieces[1];
}

Pollard::InternalNode::~InternalNode() {}
