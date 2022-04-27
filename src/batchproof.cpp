#include "../include/batchproof.h"
#include "../include/accumulator.h"
#include "crypto/common.h"
#include "state.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <tuple>

namespace utreexo {

bool BatchProof::CheckSanity(uint64_t num_leaves) const
{
    ForestState state(num_leaves);

    if (!state.CheckTargetsSanity(m_sorted_targets)) {
        return false;
    }

    std::vector<uint64_t> proof_positions;
    std::tie(proof_positions, std::ignore) = state.ProofPositions(m_sorted_targets);
    return proof_positions.size() >= m_proof.size();
}

bool BatchProof::operator==(const BatchProof& other)
{
    return m_targets.size() == other.m_targets.size() &&
           m_proof.size() == other.m_proof.size() &&
           m_targets == other.m_targets && m_proof == other.m_proof;
}

const std::vector<uint64_t>& BatchProof::GetTargets() const { return m_targets; }

const std::vector<uint64_t>& BatchProof::GetSortedTargets() const { return m_sorted_targets; }

const std::vector<std::array<uint8_t, 32>>& BatchProof::GetHashes() const { return m_proof; }

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

}; // namespace utreexo
