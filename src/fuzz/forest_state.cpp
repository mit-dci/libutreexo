#include "fuzz.h"
#include "state.h"
#include <cassert>

using namespace utreexo;

FUZZ(forest_state)
{
    FUZZ_CONSUME(uint64_t, num_leaves)
    ForestState state(num_leaves);

    state.NumRoots();
    state.NumRows();

    state.RootPositions();

    FUZZ_CONSUME(uint16_t, num_targets)
    FUZZ_CONSUME_VEC(uint64_t, targets, num_targets);

    std::sort(targets.begin(), targets.end());
    state.ProofPositions(targets);
    state.Transform(targets);
    state.CheckTargetsSanity(targets);
    state.UndoTransform(targets);

    // TODO: figure out why this makes the target so much slower
    /*for (uint64_t target : targets) {
        state.Path(target);
    }*/
}
