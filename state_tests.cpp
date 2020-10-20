#include <boost/test/unit_test.hpp>
#include <state.h>

BOOST_AUTO_TEST_SUITE(ForestStateTests)

BOOST_AUTO_TEST_CASE(constructor)
{
    ForestState state;
    BOOST_CHECK(state.numLeaves == 0);
    ForestState state1(100);
    BOOST_CHECK(state1.numLeaves == 100);
}

BOOST_AUTO_TEST_CASE(positions)
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

    BOOST_CHECK(state.leftChild(28) == 24);
    BOOST_CHECK(state.sibling(state.leftChild(28)) == 25);
    BOOST_CHECK(state.rightSibling(state.leftChild(28)) == 25);
    BOOST_CHECK(state.rightSibling(25) == 25);
    BOOST_CHECK(state.parent(state.leftChild(28)) == 28);

    for (uint64_t pos = 0; pos < 8; pos++) {
        BOOST_CHECK(state.ancestor(pos, 3) == 28);
        BOOST_CHECK(state.leftDescendant(state.ancestor(pos, 3), 3) == 0);
    }

    BOOST_CHECK(state.leftDescendant(26, 2) == 8);
    BOOST_CHECK(state.leftDescendant(25, 2) == 4);
    BOOST_CHECK(state.cousin(4) == 6);
    BOOST_CHECK(state.cousin(5) == 7);
}

BOOST_AUTO_TEST_CASE(change)
{
    ForestState state;
    for (int i = 0; i < 100; i++) {
        state.add(1);
    }
    BOOST_CHECK(state.numLeaves == 100);
    for (int i = 0; i < 100; i++) {
        state.remove(1);
    }
    BOOST_CHECK(state.numLeaves == 0);
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
    std::vector<uint64_t> expectedProof = {1, 17, 25};
    std::vector<uint64_t> expectedComputed = {0, 16, 24, 28};
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>> output = state.proofPositions(targets);
    BOOST_CHECK_EQUAL_COLLECTIONS(expectedProof.begin(), expectedProof.end(),
                                  output.first.begin(), output.first.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(expectedComputed.begin(), expectedComputed.end(),
                                  output.second.begin(), output.second.end());

    targets = {0, 2, 3, 6, 8, 10, 11};
    expectedProof = {1, 7, 9, 18};
    expectedComputed = {0, 2, 3, 6, 8, 10, 11, 16, 17, 19, 20, 21, 24, 25, 26, 28};
    output = state.proofPositions(targets);
    BOOST_CHECK_EQUAL_COLLECTIONS(expectedProof.begin(), expectedProof.end(),
                                  output.first.begin(), output.first.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(expectedComputed.begin(), expectedComputed.end(),
                                  output.second.begin(), output.second.end());

    // TODO: add tests with random numbers
}

BOOST_AUTO_TEST_SUITE_END()
