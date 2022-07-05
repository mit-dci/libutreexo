#include "impl.h"

#include <map>
#include <queue>
#include <vector>

namespace utreexo {
bool AccumulatorImpl::IngestProof(std::map<uint64_t, InternalNode*>& verification_map, const BatchProof<Hash>& proof, const std::vector<Hash>& target_hashes)
{
    if (proof.GetTargets().size() != target_hashes.size()) return false;

    std::vector<uint64_t> proof_positions = m_current_state.SimpleProofPositions(proof.GetSortedTargets());
    if (proof_positions.size() != proof.GetHashes().size()) return false;

    // Write targets
    for (int i = 0; i < proof.GetTargets().size(); ++i) {
        const uint64_t pos{proof.GetSortedTargets()[i]};
        const Hash& hash{target_hashes[i]};

        InternalNode* new_node = WriteNode(pos, hash);
        if (!new_node) return false;

        verification_map.emplace(pos, new_node);
    }

    // Write proof
    for (int i = 0; i < proof.GetHashes().size(); ++i) {
        const uint64_t pos{proof_positions[i]};
        const Hash& hash{proof.GetHashes()[i]};

        InternalNode* proof_node = WriteNode(pos, hash);
        if (!proof_node) return false;

        verification_map.emplace(pos, proof_node);
    }

    return true;
}

/** Implement `Accumulator` interface */

bool AccumulatorImpl::Verify(const BatchProof<Hash>& proof, const std::vector<Hash>& target_hashes)
{
    // TODO sanity checks

    // Ingest the proof, popualting the accumulator with target and proof
    // hashes. `IngestProof` will not overwrite existing hashes and an
    // existing hash has to match the passed one for the population to
    // succeed.
    std::map<uint64_t, InternalNode*> unverified;
    bool ingest_ok{IngestProof(unverified, proof, target_hashes)};

    // Mark all newly inserted leaves as memorable. This is necessary so
    // that they are later provable using `Prove`.
    std::queue<InternalNode*> new_leaves;
    for (int i = 0; i < target_hashes.size() && ingest_ok; ++i) {
        bool was_cached{HasMemorableMarker(target_hashes[i])};
        MarkLeafAsMemorable(unverified[proof.GetSortedTargets()[i]]);

        if (!was_cached) {
            new_leaves.push(m_cached_leaves.at(target_hashes[i]));
        }
    }

    bool verify_ok{ingest_ok};
    while (verify_ok && !unverified.empty()) {
        auto node_it{unverified.begin()};
        uint64_t node_pos{node_it->first};
        const InternalNode* node{node_it->second};

        unverified.erase(node_it);

        if (!node->m_aunt) {
            // This is a root target. We can ignore it here, `IngestProof` made
            // sure that the root hash was not overwritten.
            continue;
        }

        uint64_t sibling_pos{m_current_state.Sibling(node_pos)};
        auto sibling_it{unverified.find(sibling_pos)};
        if (sibling_it == unverified.end()) {
            verify_ok = false;
            break;
        }
        const InternalNode* sibling{sibling_it->second};

        unverified.erase(sibling_it);

        InternalNode* parent{GuaranteeParent(*node, m_current_state.Parent(node_pos))};
        if (!parent) {
            verify_ok = false;
            break;
        }

        // Quick hack to sort the children for hashing.
        const InternalNode* children[2];
        children[node_pos & 1] = node;
        children[sibling_pos & 1] = sibling;

        Hash computed_hash = ParentHash(children[0]->m_hash, children[1]->m_hash);
        if (parent->m_hash != NULL_HASH && parent->m_hash != computed_hash) {
            // The parent was cached and the newly computed hash did not match the
            // cached one, so verification fails.
            verify_ok = false;
            break;
        }

        if (parent->m_aunt) {
            // Don't add roots to the unverified set.
            unverified.emplace(m_current_state.Parent(node_pos), parent);
        }

        parent->m_hash = computed_hash;

        MaybePruneNieces(*node->m_aunt);
    }

    if (!verify_ok) {
        while (!new_leaves.empty()) {
            InternalNode* leaf{new_leaves.front()};
            new_leaves.pop();

            // Remove memorable marker.
            RemoveMemorableMarkerFromLeaf(leaf->m_hash);
            // We need to prune away all newly inserted nodes in the
            // accumulator if verification fails, so that invalid hashes are
            // not inserted into the accumulator.
            PruneBranch(*leaf);
        }
        return false;
    }

    return true;
}

} // namespace utreexo
