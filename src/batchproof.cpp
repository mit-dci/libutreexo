#include "../include/batchproof.h"
#include "../include/accumulator.h"
#include "crypto/common.h"
#include "state.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <tuple>

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
    bytes.resize(4 + 4 + m_targets.size() * 4 + m_proof.size() * 32);

    int data_offset = 0;
    WriteBE32(bytes.data(), uint32_t(m_targets.size()));
    data_offset += 4;
    WriteBE32(bytes.data() + data_offset, uint32_t(m_proof.size()));
    data_offset += 4;

    for (const uint64_t target : m_targets) {
        WriteBE32(bytes.data() + data_offset, uint32_t(target));
        data_offset += 4;
    }

    for (const Hash& hash : m_proof) {
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

    if (bytes.size() != 8ULL + num_targets * 4ULL + num_hashes * 32ULL) {
        return false;
    }

    m_targets.clear();
    m_proof.clear();
    m_targets.reserve(num_targets);
    m_sorted_targets.reserve(num_targets);
    m_proof.reserve(num_hashes);

    for (uint32_t i = 0; i < num_targets; ++i) {
        m_targets.push_back(uint64_t(ReadBE32(bytes.data() + data_offset)));
        m_sorted_targets.push_back(uint64_t(ReadBE32(bytes.data() + data_offset)));
        data_offset += 4;
    }

    std::sort(m_sorted_targets.begin(), m_sorted_targets.end());

    for (uint32_t i = 0; i < num_hashes; ++i) {
        Hash hash;
        std::memcpy(hash.data(), bytes.data() + data_offset, 32);
        data_offset += 32;
        m_proof.push_back(hash);
    }

    assert(data_offset == bytes.size());

    return true;
}

bool BatchProof::CheckSanity(uint64_t num_leaves) const
{
    ForestState state(num_leaves);

    if (!state.CheckTargetsSanity(m_sorted_targets)) {
        return false;
    }

    std::vector<uint64_t> proof_positions, tmp;
    std::tie(proof_positions, tmp) = state.ProofPositions(m_sorted_targets);
    return proof_positions.size() >= m_proof.size();
}

bool BatchProof::operator==(const BatchProof& other)
{
    return m_targets.size() == other.m_targets.size() && m_proof.size() == other.m_proof.size() &&
           m_targets == other.m_targets && m_proof == other.m_proof;
}

void BatchProof::Print()
{
    std::cout << "targets: ";
    print_vector(m_targets);

    std::cout << "proof: ";
    for (const Hash& hash : m_proof) {
        std::cout << HexStr(hash) << ", ";
    }

    std::cout << std::endl;
}

const std::vector<uint64_t>& BatchProof::GetTargets() const { return m_targets; }

const std::vector<uint64_t>& BatchProof::GetSortedTargets() const { return m_sorted_targets; }

const std::vector<std::array<uint8_t, 32>>& BatchProof::GetHashes() const { return m_proof; }

void UndoBatch::Serialize(std::vector<uint8_t>& bytes) const
{
    // num adds: 4 bytes
    // numm dels: 4 bytes
    // del positions: 4bytes * num dels
    // del hashes: 32bytes * num dels
    bytes.resize(4 + 4 + 4 * m_deleted_positions.size() + 32 * m_deleted_hashes.size());

    int data_offset = 0;
    WriteBE32(bytes.data(), uint32_t(m_num_additions));
    data_offset += 4;
    WriteBE32(bytes.data() + data_offset, uint32_t(m_deleted_positions.size()));
    data_offset += 4;

    for (const uint64_t& target : m_deleted_positions) {
        WriteBE32(bytes.data() + data_offset, uint32_t(target));
        data_offset += 4;
    }

    for (const Hash& hash : m_deleted_hashes) {
        std::memcpy(bytes.data() + data_offset, hash.data(), 32);
        data_offset += 32;
    }
}

bool UndoBatch::Unserialize(const std::vector<uint8_t>& bytes)
{
    if (bytes.size() < 8) {
        // 8 byte minmum for the number of additions and number of targets
        return false;
    }

    int data_offset = 0;
    m_num_additions = static_cast<uint64_t>(ReadBE32(bytes.data()));
    data_offset += 4;
    uint32_t num_targets = ReadBE32(bytes.data() + data_offset);
    data_offset += 4;

    if (bytes.size() != 4 + 4 + 4 * num_targets + 32 * num_targets) {
        return false;
    }

    m_deleted_positions.clear();
    m_deleted_hashes.clear();
    m_deleted_positions.reserve(num_targets);
    m_deleted_hashes.reserve(num_targets);

    for (uint32_t i = 0; i < num_targets; ++i) {
        m_deleted_positions.push_back(static_cast<uint64_t>(ReadBE32(bytes.data() + data_offset)));
        data_offset += 4;
    }

    for (uint32_t i = 0; i < num_targets; ++i) {
        Hash hash;
        std::memcpy(hash.data(), bytes.data() + data_offset, 32);
        data_offset += 32;
        m_deleted_hashes.push_back(hash);
    }

    assert(data_offset == bytes.size());

    return true;
}

uint64_t UndoBatch::GetNumAdds() const { return m_num_additions; }

const std::vector<uint64_t>& UndoBatch::GetDeletedPositions() const { return m_deleted_positions; }

const std::vector<std::array<uint8_t, 32>>& UndoBatch::GetDeletedHashes() const { return m_deleted_hashes; }

bool UndoBatch::operator==(const UndoBatch& other)
{
    return m_num_additions == other.m_num_additions &&
           m_deleted_positions.size() == other.m_deleted_positions.size() &&
           m_deleted_hashes.size() == other.m_deleted_hashes.size() &&
           m_deleted_positions == other.m_deleted_positions &&
           m_deleted_hashes == other.m_deleted_hashes;
}

void UndoBatch::Print()
{
    std::cout << "prev num adds: " << m_num_additions << std::endl;
    std::cout << "deleted positions: ";
    print_vector(m_deleted_positions);

    std::cout << "deleted hashes: ";
    for (const Hash& hash : m_deleted_hashes) {
        std::cout << HexStr(hash) << ", ";
    }

    std::cout << std::endl;
}

}; // namespace utreexo
