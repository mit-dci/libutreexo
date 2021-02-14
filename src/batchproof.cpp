#include "../include/batchproof.h"
#include "../include/accumulator.h"
#include "crypto/common.h"
#include "state.h"
#include <cassert>
#include <cstring>
#include <iostream>

namespace utreexo {

// https://github.com/bitcoin/bitcoin/blob/7f653c3b22f0a5267822eec017aea6a16752c597/src/util/strencodings.cpp#L580
template <class T>
std::string HexStr(const T s)
{
    std::string rv;
    static constexpr char hexmap[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    rv.reserve(s.size() * 2);
    for (uint8_t v : s) {
        rv.push_back(hexmap[v >> 4]);
        rv.push_back(hexmap[v & 15]);
    }
    return rv;
}

void BatchProof::Serialize(std::vector<uint8_t>& bytes) const
{
    // Number of targets:       4 bytes
    // Number of proof hashes:  4 bytes
    // Targets:                 4 bytes each
    // Proof hashes:           32 bytes each
    bytes.resize(4 + 4 + targets.size() * 4 + this->proof.size() * 32);

    int data_offset = 0;
    WriteBE32(bytes.data(), uint32_t(targets.size()));
    data_offset += 4;
    WriteBE32(bytes.data() + data_offset, uint32_t(this->proof.size()));
    data_offset += 4;

    for (const uint64_t target : targets) {
        WriteBE32(bytes.data() + data_offset, uint32_t(target));
        data_offset += 4;
    }

    for (const Hash& hash : this->proof) {
        std::memcpy(bytes.data() + data_offset, hash.data(), 32);
        data_offset += 32;
    }
}

bool BatchProof::Unserialize(const std::vector<uint8_t>& bytes)
{
    if (bytes.size() < 8) {
        // 8 byte minimum for the number of targets and proof hashses
        return false;
    }

    int data_offset = 0;
    uint32_t num_targets = ReadBE32(bytes.data());
    data_offset += 4;
    uint32_t num_hashes = ReadBE32(bytes.data() + data_offset);
    data_offset += 4;

    if (bytes.size() != 8 + num_targets * 4 + num_hashes * 32) {
        return false;
    }

    targets.clear();
    this->proof.clear();
    targets.reserve(num_targets);
    this->proof.reserve(num_hashes);

    for (uint32_t i = 0; i < num_targets; ++i) {
        targets.push_back(uint64_t(ReadBE32(bytes.data() + data_offset)));
        data_offset += 4;
    }

    for (uint32_t i = 0; i < num_hashes; ++i) {
        Hash hash;
        std::memcpy(hash.data(), bytes.data() + data_offset, 32);
        data_offset += 32;
        this->proof.push_back(hash);
    }

    assert(data_offset == bytes.size());

    return true;
}

bool BatchProof::operator==(const BatchProof& other)
{
    return targets.size() == other.targets.size() && this->proof.size() == other.proof.size() &&
           targets == other.targets && this->proof == other.proof;
}

void BatchProof::Print()
{
    std::cout << "targets: ";
    print_vector(this->targets);

    std::cout << "proof: ";
    for (const Hash& hash : proof) {
        std::cout << HexStr(hash) << ", ";
    }

    std::cout << std::endl;
}

const std::vector<uint64_t>& BatchProof::GetTargets() const { return targets; }

}; // namespace utreexo
