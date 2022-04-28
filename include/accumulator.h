#ifndef UTREEXO_ACCUMULATOR_H
#define UTREEXO_ACCUMULATOR_H

#include <array>
#include <memory>
#include <stdint.h>
#include <vector>

namespace utreexo {
// TODO: add the hash type to the Accumulator template.
using Hash = std::array<uint8_t, 32>;

template <typename H>
class BatchProof;
template <typename H>
class UndoBatch;

class Accumulator
{
public:
    using Leaves = std::vector<std::pair<Hash, bool>>;
    using Targets = std::vector<uint64_t>;
    using Hashes = std::vector<Hash>;

    virtual ~Accumulator() {}

    /**
     * Verify the existence of multiple leaves, given their hashes
     * (`target_hashes`) and an inclusion `proof`.
     *
     * `target_hashes` needs to be in ascending order accroding to the leaf
     * positions.
     */
    [[nodiscard]] virtual bool Verify(const BatchProof<Hash>& proof,
                                      const Hashes& target_hashes) = 0;

    /**
     * Modify the accumulator by adding new leaves and deleting targets.
     *
     * The positions specified for deletion (`targets`) have to be cached in
     * the accumulator for the deletion to succeed. Leaves will be cached if
     * they were added as memorable during a modification  or if they were
     * ingested during verification. The positions also need to be sorted in
     * ascending order.
     *
     * Return whether or not modifying the accumulator succeeded.
     */
    [[nodiscard]] virtual bool Modify(const Leaves& new_leaves,
                                      const Targets& targets) = 0;

    /** Undo a previous modification to the accumulator. */
    [[nodiscard]] virtual bool Undo(const uint64_t previous_num_leaves,
                                    const Hashes& previous_roots,
                                    const BatchProof<Hash>& previous_proof,
                                    const Hashes& previous_targets) = 0;

    /**
     * Prove the existence of a set of targets (leaf hashes).
     *
     * Only the existence of cached leaves can be proven. Leaves will be cached
     * if they were added as memorable during a modification or if they were
     * ingested during verification.
     *
     * Return whether or not the proof could be created.
     */
    [[nodiscard]] virtual bool Prove(BatchProof<Hash>& proof,
                                     const Hashes& target_hashes) const = 0;

    /**
     * Uncache a leaf from the accumulator. This can not fail as the leaf will
     * either be forgotton or the leaf did not exist in the first place in
     * which case there is nothing todo.
     */
    virtual void Uncache(const Hash& leaf_hash) = 0;

    /** Check whether or not a leaf is cached in the accumulator. */
    virtual bool IsCached(const Hash& leaf_hash) const = 0;

    /** Return all cached leaf hashes. */
    virtual Hashes GetCachedLeaves() const = 0;

    /** Return the state (number of leaves, merkle forest roots) of the accumulator. */
    virtual std::tuple<uint64_t, Hashes> GetState() const = 0;
};

std::unique_ptr<Accumulator> Make(const uint64_t num_leaves,
                                  const std::vector<Hash>& roots);
std::unique_ptr<Accumulator> MakeEmpty();

} // namespace utreexo

#endif
