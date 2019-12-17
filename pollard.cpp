#include <pollard.h>

#include <forest.h>

#include <uint256.h>

void Pollard::Modify(const std::vector<LeafTXO>& adds, const std::vector<uint64_t>& dels)
{
    // TODO
}

std::string Pollard::Stats() const
{
    // TODO
    return {};
}

void Pollard::add(const std::vector<LeafTXO>& adds)
{
    // TODO
}

bool Pollard::addOne(const uint256& add, bool remember)
{
    // TODO
    return false;
}

bool Pollard::rem(const std::vector<uint64_t>& dels)
{
    // TODO
    return false;
}

void Pollard::moveNode(const arrow& a)
{
    // TODO
}

void Pollard::reHashOne(uint64_t pos)
{
    // TODO
}

Forest Pollard::toFull()
{
    // TODO
    return {};
}

void Pollard::IngestBlockProof(const BlockProof& bp)
{
    // TODO
}

uint8_t Pollard::height()
{
    // TODO
    return 0;
}

std::vector<uint256> Pollard::topHashesReverse()
{
    // TODO
    return {};
}

std::pair<std::vector<Pollard::tops_iter>, std::vector<Pollard::tops_iter>> Pollard::descendToPos(uint64_t)
{
    // TODO
    return {{},{}};
}
