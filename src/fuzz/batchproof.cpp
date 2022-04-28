#include "batchproof.h"
#include "fuzz.h"
#include <cassert>

using namespace utreexo;

FUZZ(batchproof)
{
    FUZZ_CONSUME(uint8_t, num_targets)
    FUZZ_CONSUME(uint8_t, num_hashes)
    FUZZ_CONSUME_VEC(uint8_t, proof_bytes, 8 + num_targets * 4 + num_hashes * 32)

    BatchProof proof;
    if (proof.Unserialize(proof_bytes)) {
        std::vector<uint8_t> bytes;
        proof.Serialize(bytes);
        assert(proof_bytes == bytes);
    }
}
