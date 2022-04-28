#include <cassert>
#include <cstring>
#include <map>
#include <optional>
#include <string>

#include "impl.h"

namespace utreexo {

std::unique_ptr<Accumulator> Make(const uint64_t num_leaves, const std::vector<Hash>& roots)
{
    return std::make_unique<AccumulatorImpl>(num_leaves, roots);
}
std::unique_ptr<Accumulator> MakeEmpty() { return Make(0, {}); }

} // namespace utreexo
