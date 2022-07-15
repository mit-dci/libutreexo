#include "utreexo.h"

#include <boost/test/unit_test.hpp>

using namespace utreexo;
using namespace utreexo::detail;

BOOST_AUTO_TEST_SUITE(state_tests)

BOOST_AUTO_TEST_CASE(constructor)
{
    ForestState state;
    BOOST_CHECK(state.m_root_bits == 0);
    ForestState state1(100);
    BOOST_CHECK(state1.m_root_bits == 100);
}

BOOST_AUTO_TEST_CASE(positions)
{
    /*
     * xx
     * |-------------------------------\
     * 28                              xx
     * |---------------\               |---------------\
     * 24              25              26              xx
     * |-------\       |-------\       |-------\       |-------\
     * 16      17      18      19      20      21      22      xx
     * |---\   |---\   |---\   |---\   |---\   |---\   |---\   |---\
     * 00  01  02  03  04  05  06  07  08  09  10  11  12  13  14  xx
     */

    /*
     *  11100
     *  |-----------------------\
     *  11000                   11001                   11010
     *  |-----------\           |-----------\           |-----------\
     *  10000       10001       10010       10011       10100       10101       10110
     *  |-----\     |-----\     |-----\     |-----\     |-----\     |-----\     |-----\
     *  00000 00001 00010 00011 00100 00101 00110 00111 01000 01001 01010 01011 01100 01101 01110
     */
    // the bits visualization helps to better understand the algorithms
    // https://github.com/mit-dci/utreexo/blob/master/accumulator/printout.txt

    ForestState state(15);

    BOOST_CHECK(state.LeftChild(28) == 24);
    BOOST_CHECK(state.Sibling(state.LeftChild(28)) == 25);
    BOOST_CHECK(state.RightSibling(state.LeftChild(28)) == 25);
    BOOST_CHECK(state.RightSibling(25) == 25);
    BOOST_CHECK(state.Parent(state.LeftChild(28)) == 28);

    for (uint64_t pos = 0; pos < 8; pos++) {
        BOOST_CHECK(state.Ancestor(pos, 3) == 28);
        BOOST_CHECK(state.LeftDescendant(state.Ancestor(pos, 3), 3) == 0);
    }

    BOOST_CHECK(state.LeftDescendant(26, 2) == 8);
    BOOST_CHECK(state.LeftDescendant(25, 2) == 4);
    BOOST_CHECK(state.Cousin(4) == 6);
    BOOST_CHECK(state.Cousin(5) == 7);
}

#define CHECK_VECTORS_EQUAL(c1, c2)                                                                    \
    {                                                                                                  \
        auto c1_copy{c1};                                                                              \
        auto c2_copy{c2};                                                                              \
        BOOST_CHECK_EQUAL_COLLECTIONS(c1_copy.begin(), c1_copy.end(), c2_copy.begin(), c2_copy.end()); \
    }

BOOST_AUTO_TEST_CASE(proof_positions)
{
    ForestState state(15);
    /*
     * xx
     * |-------------------------------\
     * 28                              xx
     * |---------------\               |---------------\
     * 24              25              26              xx
     * |-------\       |-------\       |-------\       |-------\
     * 16      17      18      19      20      21      22      xx
     * |---\   |---\   |---\   |---\   |---\   |---\   |---\   |---\
     * 00  01  02  03  04  05  06  07  08  09  10  11  12  13  14  xx
     */

    CHECK_VECTORS_EQUAL(state.SimpleProofPositions({}), std::vector<uint64_t>{});
    CHECK_VECTORS_EQUAL(state.SimpleProofPositions({0}), std::vector<uint64_t>({1, 17, 25}));
    CHECK_VECTORS_EQUAL(state.SimpleProofPositions({14, 22, 26, 28}), std::vector<uint64_t>({}));
    CHECK_VECTORS_EQUAL(state.SimpleProofPositions({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}), std::vector<uint64_t>({}));
    CHECK_VECTORS_EQUAL(state.SimpleProofPositions({0, 2, 4, 6, 8, 10, 12, 14}), std::vector<uint64_t>({1, 3, 5, 7, 9, 11, 13}));
    CHECK_VECTORS_EQUAL(state.SimpleProofPositions({0, 12, 20, 25}), std::vector<uint64_t>({1, 13, 17, 21}));
    CHECK_VECTORS_EQUAL(state.SimpleProofPositions({4, 24}), std::vector<uint64_t>({5, 19}));
}

BOOST_AUTO_TEST_SUITE_END()
