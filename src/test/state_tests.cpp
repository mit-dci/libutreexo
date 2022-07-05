#include "../state.h"
#include <boost/test/unit_test.hpp>

using namespace utreexo;

BOOST_AUTO_TEST_SUITE(state_tests)

BOOST_AUTO_TEST_CASE(constructor)
{
    ForestState state;
    BOOST_CHECK(state.m_num_leaves == 0);
    ForestState state1(100);
    BOOST_CHECK(state1.m_num_leaves == 100);
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

BOOST_AUTO_TEST_CASE(proof)
{
    /*
     * 28
     * |---------------\
     * 24              25              26
     * |-------\       |-------\       |-------\
     * 16      17      18      19      20      21      22
     * |---\   |---\   |---\   |---\   |---\   |---\   |---\
     * 00  01  02  03  04  05  06  07  08  09  10  11  12  13  14
     */

    ForestState state(15);

    std::vector<uint64_t> targets = {0};
    std::vector<uint64_t> expected_proof = {1, 17, 25};
    std::vector<uint64_t> expected_computed = {0, 16, 24, 28};
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>> output = state.ProofPositions(targets);
    BOOST_CHECK_EQUAL_COLLECTIONS(expected_proof.begin(), expected_proof.end(),
                                  output.first.begin(), output.first.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(expected_computed.begin(), expected_computed.end(),
                                  output.second.begin(), output.second.end());

    targets = {0, 2, 3, 6, 8, 10, 11};
    expected_proof = {1, 7, 9, 18};
    expected_computed = {0, 2, 3, 6, 8, 10, 11, 16, 17, 19, 20, 21, 24, 25, 26, 28};
    output = state.ProofPositions(targets);
    BOOST_CHECK_EQUAL_COLLECTIONS(expected_proof.begin(), expected_proof.end(),
                                  output.first.begin(), output.first.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(expected_computed.begin(), expected_computed.end(),
                                  output.second.begin(), output.second.end());

    // TODO: add tests with random numbers
}

BOOST_AUTO_TEST_SUITE_END()
