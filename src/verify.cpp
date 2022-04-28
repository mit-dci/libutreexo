#include "impl.h"

namespace utreexo {
bool AccumulatorImpl::IngestProof(std::vector<InternalNode*>& target_nodes, const BatchProof<Hash>& proof, const std::vector<Hash>& target_hashes)
{
    std::vector<uint64_t> proof_positions = m_current_state.SimpleProofPositions(proof.GetSortedTargets());

    if (proof_positions.size() != proof.GetHashes().size()) return false;
    if (proof.GetTargets().size() != target_hashes.size()) return false;

    // Write targets
    for (int i = 0; i < proof.GetTargets().size(); ++i) {
        const uint64_t pos{proof.GetTargets()[i]};
        const Hash& hash{target_hashes[i]};

        InternalNode* new_node = WriteNode(pos, hash);
        if (!new_node) return false;

        target_nodes.push_back(new_node);
    }

    // Write proof
    for (int i = 0; i < proof.GetHashes().size(); ++i) {
        const uint64_t pos{proof_positions[i]};
        const Hash& hash{proof.GetHashes()[i]};
        if (!WriteNode(pos, hash)) return false;
    }

    return true;
}

bool AccumulatorImpl::VerifyRow(std::vector<NodeAndMetadata>::iterator start, std::vector<NodeAndMetadata>::iterator end, std::vector<NodeAndMetadata>& next_row, uint16_t root_counts[64])
{
    for (; start < end; ++start) {
        InternalNode& node = start->GetNode();
        InternalNode* aunt{node.m_aunt};
        if (!aunt) {
            // Only roots have no aunts, so the node is a root and the
            // computed hash has to match the root hash.
            if (ParentHash(node.m_nieces[0]->m_hash, node.m_nieces[1]->m_hash) != node.m_hash) return false;
            if (!start->HasMemorableChild()) MaybePruneNieces(node);
            ++root_counts[start->GetRootIndex()];
            continue;
        }

        // Get the sibling to be able to compute the hash of the node.
        const uint8_t lr_sib{aunt->m_nieces[0] == &node ? (uint8_t)1 : (uint8_t)0};
        InternalNode* sibling{aunt->m_nieces[lr_sib]};
        CHECK_SAFE(sibling && sibling->m_nieces[0] && sibling->m_nieces[1]);

        Hash computed_hash = ParentHash(sibling->m_nieces[0]->m_hash, sibling->m_nieces[1]->m_hash);

        if (!start->HasMemorableChild()) MaybePruneNieces(*sibling);

        if (node.m_hash != NULL_HASH) {
            // This node already has a hash, the computed hash has to match
            // or the given proof was invalid.
            if (computed_hash != node.m_hash) return false;
        } else {
            // Instead of calling ReHash we can just asign the
            // computed hash hash as it will be the same.
            node.m_hash = computed_hash;
        }

        // Get the parent and append it to the next row.

        const uint64_t parent_pos{m_current_state.Parent(start->GetPosition())};

        InternalNode* grand_aunt{aunt->m_aunt};
        if (!grand_aunt) {
            // aunt is a root and therefore the parent of the node.
            if (next_row.size() == 0 || &next_row.back().GetNode() != aunt) next_row.emplace_back(*aunt, parent_pos, false, start->GetRootIndex());
            continue;
        }

        const uint8_t lr_parent{(uint8_t)(parent_pos & 1)};
        InternalNode* parent{GuaranteeNiece(*grand_aunt, lr_parent)};

        if (next_row.size() == 0 || &next_row.back().GetNode() != parent) {
            next_row.emplace_back(*parent, parent_pos, false, start->GetRootIndex());
        }
    }

    return true;
}

/** Implement `Accumulator` interface */

bool AccumulatorImpl::Verify(const BatchProof<Hash>& proof, const std::vector<Hash>& target_hashes)
{
    // TODO sanity checks

    // Ingest the proof, popualting the accumulator with target and proof
    // hashes. `IngestProof` will not overwrite existsing hashes and an
    // existing hash has to match the passed one for the population to
    // succeed.
    std::vector<InternalNode*> target_nodes;
    bool ingest_ok{IngestProof(target_nodes, proof, target_hashes)};

    // Expected indices of the roots that should be compared against during
    // verification.
    std::vector<int> expected_root_indices;
    expected_root_indices.reserve(MAX_TREE_HEIGHT);
    // Passed to VerifyRow to count roots that are reached during
    // verification. Later used to check for expected root indices.
    uint16_t root_counts[MAX_TREE_HEIGHT];
    std::memset(root_counts, 0, sizeof(root_counts));

    // Get the parents of all targets and mark them as "unverified" meaning
    // they will have to be rehashes until a root is reached.
    // `unverified` will contain targets across multiple rows.
    std::vector<NodeAndMetadata> unverified;
    for (int i = 0; i < target_nodes.size(); ++i) {
        const uint64_t parent_pos{m_current_state.Parent(proof.GetTargets()[i])};
        InternalNode* parent = GuaranteeParent(*target_nodes[i], parent_pos);
        if (parent && (unverified.empty() || &unverified.back().GetNode() != parent)) {
            uint8_t root_index{0};
            std::tie(root_index, std::ignore, std::ignore) = m_current_state.Path(parent_pos);
            unverified.emplace_back(*parent, parent_pos, true, root_index);

            if (expected_root_indices.empty() || expected_root_indices.back() != root_index) expected_root_indices.push_back(root_index);
        }
    }

    uint8_t row{unverified.empty() ? (uint8_t)0 : m_current_state.DetectRow(unverified.front().GetPosition())};
    const ForestState state_copy{m_current_state};
    // Lambda that is used to find the next row in `unverified`.
    auto row_change = [state_copy, &row](const NodeAndMetadata& node_and_pos) {
        return state_copy.DetectRow(node_and_pos.GetPosition()) > row;
    };

    auto row_start = unverified.begin();
    auto row_end = std::find_if(unverified.begin(), unverified.end(), row_change);
    std::vector<NodeAndMetadata> next_row{row_start, row_end};

    bool verify_ok{ingest_ok};
    for (; verify_ok && (!next_row.empty() || row_start != row_end); ++row) {
        row = m_current_state.DetectRow(next_row.front().GetPosition());

        std::vector<NodeAndMetadata> parents;
        if (!VerifyRow(next_row.begin(), next_row.end(), parents, root_counts)) {
            verify_ok = false;
            break;
        }

        row_start = row_end;

        if (row_start == unverified.end()) {
            // There are no more nodes in `unverified` so all that is left
            // are the parents.
            next_row = parents;
        } else {
            // Merge nodes from the next row in `unverified` with the
            // parents if they are on the same row.
            uint8_t next_unverified_row = m_current_state.DetectRow(row_end->GetPosition());
            if (next_unverified_row == row + 1) {
                row_end = std::find_if(row_end, unverified.end(), row_change);
            }
            next_row.clear();
            std::merge(parents.begin(), parents.end(),
                       row_start, row_end,
                       std::back_inserter(next_row),
                       CompareNodeAndMetadataByPosition);
        }
    }

    // Belt and suspender check for expected root indices.
    for (int expected_root_index : expected_root_indices) {
        CHECK(root_counts[expected_root_index] < 2);
        if (root_counts[expected_root_index] == 0) {
            verify_ok = false;
        }
    }

    if (!verify_ok) {
        // We need to prune away all newly inserted nodes in the
        // accumulator if verification fails, so that invalid hashes are
        // not inserted into the accumulator.
        for (InternalNode* target_node : target_nodes) {
            PruneBranch(*target_node);
        }

        return false;
    }

    // Mark all newly inserted leaves as memorable. This is necessary so
    // that they are later proovable using `Prove`.
    for (InternalNode* target_node : target_nodes) {
        MarkLeafAsMemorable(target_node);
    }

    return true;
}
} // namespace utreexo

