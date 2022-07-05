#include "impl.h"

namespace utreexo {

void AccumulatorImpl::UndoOne(InternalNode& parent, InternalNode& aunt, const uint8_t lr_sib)
{
    InternalNode* node{new InternalNode(NULL_HASH)};
    node->m_nieces[0] = aunt.m_nieces[0];
    node->m_nieces[1] = aunt.m_nieces[1];
    SetAuntForNieces(*node);

    aunt.m_nieces[lr_sib] = new InternalNode(parent.m_hash);
    aunt.m_nieces[lr_sib ^ 1] = node;
    SetAuntForNieces(aunt);

    if (HasMemorableMarker(parent.m_hash)) {
        OverwriteMemorableMarker(parent.m_hash, aunt.m_nieces[lr_sib]);
    }
}

bool AccumulatorImpl::Undo(const uint64_t previous_num_leaves,
                           const Hashes& previous_root_hashes,
                           const BatchProof<Hash>& previous_proof,
                           const Hashes& previous_targets)
{
    // TODO
    return false;
}

} // namespace utreexo
