#include <omnicore/omnicore.h>
#include <omnicore/nftdb.h>

#include <stdint.h>
#include <string>
#include <utility>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(omnicore_nftdb_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(nftdb_test_sequence)
{
    LOCK(cs_tally);
    std::unique_ptr<CMPNonFungibleTokensDB> UITDb{new CMPNonFungibleTokensDB(GetDataDir() / "OMNI_nftdb", true)};

    /* Test A:
     * - Create 1000 tokens of prop 50 and assign them to address 'Alice'
     * - Check that returned range is 1,1000
     * - Check that the owner of token ID 1 of prop 50 is address 'Alice'
     * - Check that the owner of token ID 1000 of prop 50 is address 'Alice'
     * - Check that the owner of token ID 454 of prop 50 is address 'Alice'
     * - Check that range of token IDs is not negative
     */
    std::pair<int64_t, int64_t> testA = UITDb->CreateNonFungibleTokens(50, 1000, "Alice", "");
    BOOST_CHECK_EQUAL(1, testA.first);
    BOOST_CHECK_EQUAL(1000, testA.second);
    BOOST_CHECK_EQUAL("Alice", UITDb->GetNonFungibleTokenValueInRange(50, 1, 1000));
    BOOST_CHECK_EQUAL("Alice", UITDb->GetNonFungibleTokenValue(50, 1000, NonFungibleStorage::RangeIndex));
    BOOST_CHECK_EQUAL("Alice", UITDb->GetNonFungibleTokenValue(50, 454, NonFungibleStorage::RangeIndex));

    std::pair<int64_t, int64_t> testA1 = UITDb->CreateNonFungibleTokens(50, -1001, "Bob", "");
    BOOST_CHECK_EQUAL(0, testA1.first);
    BOOST_CHECK_EQUAL(0, testA1.second);

    /* Test B:
     * - Create another 1000 tokens of prop 50 and assign them to address 'Alice'
     * - Check that the returned range is 1001,2000
     * - Check that the owner of token ID 1001 of prop 50 is address 'Alice'
     * - Check that the owner of token ID 2000 of prop 50 is address 'Alice'
     * - Check that the ranges were merged and address 'Alice' owns a contigous range 1-2000
     */
    std::pair<int64_t, int64_t> testB = UITDb->CreateNonFungibleTokens(50, 1000, "Alice", "");
    BOOST_CHECK_EQUAL(1001, testB.first);
    BOOST_CHECK_EQUAL(2000, testB.second);
    BOOST_CHECK_EQUAL("Alice", UITDb->GetNonFungibleTokenValueInRange(50, 1001, 2000));

    /* Test C:
     * - Create another 1000 tokens of prop 50 and assign them to address 'Bob'
     * - Check that the returned range is 2001,3000
     * - Check that the owner of token ID 2001 of prop 50 is address 'Bob'
     * - Check that the owner of token ID 3000 of prop 50 is address 'Bob'
     * - Check that the ranges were not merged (check token ID 2000-2001 is not a contiguous range)
     */
    std::pair<int64_t, int64_t> testC = UITDb->CreateNonFungibleTokens(50, 1000, "Bob", "");
    BOOST_CHECK_EQUAL(3000, testC.second);
    BOOST_CHECK_EQUAL(2001, testC.first);
    BOOST_CHECK_EQUAL("Bob", UITDb->GetNonFungibleTokenValueInRange(50, 2001, 3000));
    BOOST_CHECK(UITDb->GetNonFungibleTokenValueInRange(50, 2000, 2001).empty());

    /* Test D:
     * - Move the entire token range 2001 to 3000 of prop 50 from address 'Bob' to address 'Charles'
     * - Check that the move was successful
     * - Check that the owner of token ID 2001 of prop 50 is address 'Charles'
     * - Check that the owner of token ID 3000 of prop 50 is address 'Charles'
     */
    BOOST_CHECK(UITDb->MoveNonFungibleTokens(50, 2001, 3000, "Bob", "Charles"));
    BOOST_CHECK_EQUAL("Charles", UITDb->GetNonFungibleTokenValueInRange(50, 2001, 3000));

    /* Test E:
     * - Create another 1000 tokens of prop 50 and assign them to address 'David'
     * - Check that the returned range is 3001,4000
     * - Check that the owner of token ID 3001 of ptop 50 is address 'David'
     * - Check that the owner of token ID 4000 of prop 50 is address 'David'
     * - Check that the ranges were not merged (check token ID 3000-3001 is not a contiguous range)
     * - Move the entire token range 3001-4000 of prop 50 from address 'David' to address 'Charles'
     * - Check that the move was successful
     * - Check that the owner of token ID 3001 of prop 50 is now address 'Charles'
     * - Check that the owner of token ID 4000 of prop 50 is now address 'Charles'
     * - Check that the ranges were merged (check token ID 3000-3001 is a contiguous range)
     */
    std::pair<int64_t, int64_t> testE = UITDb->CreateNonFungibleTokens(50, 1000, "David", "");
    BOOST_CHECK_EQUAL(3001, testE.first);
    BOOST_CHECK_EQUAL(4000, testE.second);
    BOOST_CHECK_EQUAL("David", UITDb->GetNonFungibleTokenValueInRange(50, 3001, 4000));
    BOOST_CHECK(UITDb->GetNonFungibleTokenValueInRange(50, 3000, 3001).empty());
    BOOST_CHECK(UITDb->MoveNonFungibleTokens(50, 3001, 4000, "David", "Charles"));
    BOOST_CHECK_EQUAL("Charles", UITDb->GetNonFungibleTokenValueInRange(50, 3001, 4000));

    /* Test F:
     * - Check that the owner of token ID 500 of prop 50 is 'Alice'
     * - Move the individual non-fungible token 500 of prop 50 from 'Alice' to 'Bob'
     * - Check that the move was successful
     * - Check that the owner of token ID 500 of prop 50 is 'Bob'
     * - TODO: Check that address 'Alice' now owns 1-499 and 501-2000 via dump
     * - TODO: Check that address 'Bob' now owns 500 and 2001-3000 via dump
     */
    BOOST_CHECK_EQUAL("Alice", UITDb->GetNonFungibleTokenValue(50, 500, NonFungibleStorage::RangeIndex));
    BOOST_CHECK(UITDb->MoveNonFungibleTokens(50, 500, 500, "Alice", "Bob"));
    BOOST_CHECK_EQUAL("Bob", UITDb->GetNonFungibleTokenValue(50, 500, NonFungibleStorage::RangeIndex));

    /* Test G:
     * - Check that the owner of token ID 2300 of prop 50 is 'Charles'
     * - Check that the owner of token ID 2400 of prop 50 is 'Charles'
     * - Check that the range 2300-2400 is contiguous
     * - Move the range of tokens 2300-2400 of prop 50 from 'Charles' to 'Bob'
     * - Check that the move was successful
     * - Check that the owner of token ID 2300 of prop 50 is 'Bob'
     * - Check that the owner of token ID 2400 of prop 50 is 'Bob'
     * - Check that the range 2300-2400 is contiguous
     */
    BOOST_CHECK_EQUAL("Charles", UITDb->GetNonFungibleTokenValueInRange(50, 2300, 2400));
    BOOST_CHECK(UITDb->MoveNonFungibleTokens(50, 2300, 2400, "Charles", "Bob"));
    BOOST_CHECK_EQUAL("Bob", UITDb->GetNonFungibleTokenValueInRange(50, 2300, 2400));
}

BOOST_AUTO_TEST_CASE(nftdb_test_rollback)
{
    LOCK(cs_tally);
    std::unique_ptr<CMPNonFungibleTokensDB> UITDb{new CMPNonFungibleTokensDB(GetDataDir() / "OMNI_nftdb_rollback", true)};

    std::pair<int64_t, int64_t> testA = UITDb->CreateNonFungibleTokens(50, 1000, "David", "");
    BOOST_CHECK_EQUAL(1, testA.first);
    BOOST_CHECK_EQUAL(1000, testA.second);
    BOOST_CHECK_EQUAL("David", UITDb->GetNonFungibleTokenValueInRange(50, 1, 1000));

    UITDb->WriteBlockCache(1);

    BOOST_CHECK(UITDb->MoveNonFungibleTokens(50, 1, 1000, "David", "Charles"));
    BOOST_CHECK_EQUAL("Charles", UITDb->GetNonFungibleTokenValueInRange(50, 1, 1000));

    UITDb->WriteBlockCache(2);

    // rollback above block 2
    UITDb->RollBackAboveBlock(2);
    BOOST_CHECK_EQUAL("David", UITDb->GetNonFungibleTokenValueInRange(50, 1, 1000));

    std::pair<int64_t, int64_t> testB = UITDb->CreateNonFungibleTokens(50, 1000, "Alice", "");
    BOOST_CHECK_EQUAL(1001, testB.first);
    BOOST_CHECK_EQUAL(2000, testB.second);
    BOOST_CHECK_EQUAL("Alice", UITDb->GetNonFungibleTokenValueInRange(50, 1001, 2000));

    UITDb->WriteBlockCache(2);

    std::pair<int64_t, int64_t> testE = UITDb->CreateNonFungibleTokens(50, 1000, "David", "");
    BOOST_CHECK_EQUAL(2001, testE.first);
    BOOST_CHECK_EQUAL(3000, testE.second);
    BOOST_CHECK_EQUAL("David", UITDb->GetNonFungibleTokenValueInRange(50, 2001, 3000));
    BOOST_CHECK(UITDb->GetNonFungibleTokenValueInRange(50, 2000, 2001).empty());

    UITDb->WriteBlockCache(3);

    BOOST_CHECK(UITDb->MoveNonFungibleTokens(50, 2001, 2100, "David", "Charles"));
    BOOST_CHECK_EQUAL("Charles", UITDb->GetNonFungibleTokenValueInRange(50, 2001, 2100));
    BOOST_CHECK_EQUAL("David", UITDb->GetNonFungibleTokenValueInRange(50, 2101, 3000));
    BOOST_CHECK(UITDb->GetNonFungibleTokenValueInRange(50, 2001, 3000).empty());

    UITDb->WriteBlockCache(4);

    // rollback above block 4
    UITDb->RollBackAboveBlock(4);
    BOOST_CHECK_EQUAL("David", UITDb->GetNonFungibleTokenValueInRange(50, 2001, 3000));

    // rollback above block 2
    UITDb->RollBackAboveBlock(2);
    BOOST_CHECK(UITDb->GetNonFungibleTokenValueInRange(50, 2001, 3000).empty());
    BOOST_CHECK(UITDb->GetNonFungibleTokenValueInRange(50, 1001, 2000).empty());
    BOOST_CHECK_EQUAL("David", UITDb->GetNonFungibleTokenValueInRange(50, 1, 1000));
}

BOOST_AUTO_TEST_SUITE_END()
