/**
 * @file nftdb.cpp
 *
 * This file contains functionality for the non-fungible tokens database.
 */

#include <omnicore/nftdb.h>

#include <omnicore/omnicore.h>
#include <omnicore/errors.h>
#include <omnicore/log.h>

#include <validation.h>

#include <climits>
#include <cstdint>
#include <stdint.h>
#include <tuple>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

typedef std::underlying_type<NonFungibleStorage>::type StorageType;

static std::string createNFTKey(uint32_t propertyId, NonFungibleStorage type = NonFungibleStorage::None, int64_t tokenIdStart = 0, int64_t tokenIdEnd = 0)
{
    return strprintf("%010d_%u_%020d-%020d", propertyId, static_cast<StorageType>(type), tokenIdStart, tokenIdEnd);
}

static std::tuple<uint32_t, NonFungibleStorage, int64_t, int64_t> parseNFTKey(const std::string& key)
{
    std::vector<std::string> vPropertyId;
    boost::split(vPropertyId, key, boost::is_any_of("_"), boost::token_compress_on);
    assert(vPropertyId.size() == 3); // if size !=3 then we cannot trust the data in the DB and we must halt

    std::vector<std::string> vRanges;
    boost::split(vRanges, vPropertyId[2], boost::is_any_of("-"), boost::token_compress_on);
    assert(vRanges.size() == 2); // if size !=2 then we cannot trust the data in the DB and we must halt

    return std::make_tuple(boost::lexical_cast<uint32_t>(vPropertyId[0]),
                           static_cast<NonFungibleStorage>(boost::lexical_cast<uint32_t>(vPropertyId[1])),
                           boost::lexical_cast<int64_t>(vRanges[0]),
                           boost::lexical_cast<int64_t>(vRanges[1]));
}

template<int I, int J, class... T>
auto to_pair(std::tuple<T...>& t) -> decltype(std::make_pair(std::get<I>(t), std::get<J>(t)))
{
    return std::make_pair(std::get<I>(t), std::get<J>(t));
}

/* Gets the range a non-fungible token is in
 */
std::pair<int64_t,int64_t> CMPNonFungibleTokensDB::GetRange(const uint32_t &propertyId, const int64_t &tokenId, const NonFungibleStorage type)
{
    assert(pdb);
    CDBaseIterator it{NewIterator(), createNFTKey(propertyId, type)};

    for (; it; ++it) {
        auto nkey = parseNFTKey(it->key().ToString());
        if (to_pair<0, 1>(nkey) != std::make_pair(propertyId, type)) {
            break;
        }
        if (tokenId >= std::get<2>(nkey) && tokenId <= std::get<3>(nkey)) {
            return to_pair<2, 3>(nkey);
        }
    }

    return std::make_pair(0, 0); // token not found, return zero'd range
}

/* Checks if the range of tokens is contiguous (ie owned by a single address)
 */
std::string CMPNonFungibleTokensDB::GetNonFungibleTokenValueInRange(const uint32_t &propertyId, const int64_t &rangeStart, const int64_t &rangeEnd)
{
    assert(pdb);
    auto rangeIndex = NonFungibleStorage::RangeIndex;
    CDBaseIterator it{NewIterator(), createNFTKey(propertyId, rangeIndex)};
    for (; it; ++it) {
        auto nkey = parseNFTKey(it->key().ToString());
        if (to_pair<0, 1>(nkey) != std::make_pair(propertyId, rangeIndex)) {
            break;
        }
        if (rangeStart >= std::get<2>(nkey) && rangeEnd <= std::get<3>(nkey)) {
            return it->value().ToString();
        }
    }

    return {}; // range doesn't exist
}

/* Moves a range of tokens (returns false if not able to move)
 */
bool CMPNonFungibleTokensDB::MoveNonFungibleTokens(const uint32_t &propertyId, const int64_t &tokenIdStart, const int64_t &tokenIdEnd, const std::string &from, const std::string &to)
{
    if (msc_debug_nftdb) PrintToLog("%s(): %d:%d:%d:%s:%s, line %d, file: %s\n", __FUNCTION__, propertyId, tokenIdStart, tokenIdEnd, from, to, __LINE__, __FILE__);

    assert(pdb);

    // check that 'from' owns both the start and end token and that the range is contiguous (owns the entire range)
    std::string startOwner = GetNonFungibleTokenValueInRange(propertyId, tokenIdStart, tokenIdEnd);
    if (startOwner != from) {
        return false;
    }

    // are we moving the complete range from 'from'?
    // we know the range is contiguous (above) so we can use a single GetRange call
    bool bMovingCompleteRange = false;
    std::pair<int64_t,int64_t> senderTokenRange = GetRange(propertyId, tokenIdStart, NonFungibleStorage::RangeIndex);
    if (senderTokenRange.first == tokenIdStart && senderTokenRange.second == tokenIdEnd) {
        bMovingCompleteRange = true;
    }

    // does 'to' have adjacent ranges that need to be merged?
    bool bToAdjacentRangeBefore = false;
    bool bToAdjacentRangeAfter = false;
    std::string rangeBelowOwner = GetNonFungibleTokenValue(propertyId, tokenIdStart - 1, NonFungibleStorage::RangeIndex);
    std::string rangeAfterOwner = GetNonFungibleTokenValue(propertyId, tokenIdEnd + 1, NonFungibleStorage::RangeIndex);
    if (rangeBelowOwner == to) {
        bToAdjacentRangeBefore = true;
    }
    if (rangeAfterOwner == to) {
        bToAdjacentRangeAfter = true;
    }

    // adjust 'from' ranges
    DeleteRange(propertyId, senderTokenRange.first, senderTokenRange.second, NonFungibleStorage::RangeIndex);
    if (bMovingCompleteRange != true) {
        if (senderTokenRange.first < tokenIdStart) {
            AddRange(propertyId, senderTokenRange.first, tokenIdStart - 1, from, NonFungibleStorage::RangeIndex);
        }
        if (senderTokenRange.second > tokenIdEnd) {
            AddRange(propertyId, tokenIdEnd + 1, senderTokenRange.second, from, NonFungibleStorage::RangeIndex);
        }
    }

    // adjust 'to' ranges
    if (bToAdjacentRangeBefore == false && bToAdjacentRangeAfter == false) {
        AddRange(propertyId, tokenIdStart, tokenIdEnd, to, NonFungibleStorage::RangeIndex);
    } else {
        int64_t newTokenIdStart = tokenIdStart;
        int64_t newTokenIdEnd = tokenIdEnd;
        if (bToAdjacentRangeBefore) {
            std::pair<int64_t,int64_t> oldRange = GetRange(propertyId, tokenIdStart-1, NonFungibleStorage::RangeIndex);
            newTokenIdStart = oldRange.first;
            DeleteRange(propertyId, oldRange.first, oldRange.second, NonFungibleStorage::RangeIndex);
        }
        if (bToAdjacentRangeAfter) {
            std::pair<int64_t,int64_t> oldRange = GetRange(propertyId, tokenIdEnd+1, NonFungibleStorage::RangeIndex);
            newTokenIdEnd = oldRange.second;
            DeleteRange(propertyId, oldRange.first, oldRange.second, NonFungibleStorage::RangeIndex);
        }
        AddRange(propertyId, newTokenIdStart, newTokenIdEnd, to, NonFungibleStorage::RangeIndex);
    }

    return true;
}

/*  Sets token data on non-fungible tokens
 */
bool CMPNonFungibleTokensDB::ChangeNonFungibleTokenData(const uint32_t &propertyId, const int64_t &tokenIdStart, const int64_t &tokenIdEnd, const std::string &data, const NonFungibleStorage type)
{
    if (msc_debug_nftdb) PrintToLog("%s(): %d:%d:%d:%s:%s, line %d, file: %s\n", __FUNCTION__, propertyId, tokenIdStart, tokenIdEnd, data, type == NonFungibleStorage::IssuerData ? "IssuerData" : "HolderData", __LINE__, __FILE__);

    assert(pdb);

    // Get all ranges in the range we are setting.
    std::set<std::pair<int64_t,int64_t>> ranges;
    for (int64_t i = tokenIdStart; i <= tokenIdEnd;) {
        auto tokenRange = GetRange(propertyId, i, type);

        // Not found, no data range set.
        if (tokenRange == std::make_pair(int64_t{0}, int64_t{0})) {
            break;
        }

        ranges.insert(tokenRange);
        i = tokenRange.second + 1;
    }

    // If we have previous ranges rewrite if needed
    if (!ranges.empty()) {
        // Get data on before and after ranges we are writing over
        auto beforeData = GetNonFungibleTokenValue(propertyId, ranges.begin()->first, type);
        auto afterData = GetNonFungibleTokenValue(propertyId, ranges.rbegin()->first, type);

        // Delete all ranges
        for (const auto& tokenRange : ranges) {
            DeleteRange(propertyId, tokenRange.first, tokenRange.second, type);
        }

        // Rewrite first range
        if (ranges.begin()->first < tokenIdStart) {
            AddRange(propertyId, ranges.begin()->first, tokenIdStart - 1, beforeData, type);
        }
        if (ranges.rbegin()->second > tokenIdEnd) {
            AddRange(propertyId, tokenIdEnd + 1, ranges.rbegin()->second, afterData, type);
        }
    }

    // Set new data
    AddRange(propertyId, tokenIdStart, tokenIdEnd, data, type);

    return true;
}

/* Counts the highest token range end (which is thus the total number of tokens)
 */
int64_t CMPNonFungibleTokensDB::GetHighestRangeEnd(const uint32_t &propertyId)
{
    assert(pdb);

    int64_t tokenCount = 0;
    auto rangeIndex = NonFungibleStorage::RangeIndex;
    CDBaseIterator it{NewIterator(), createNFTKey(propertyId, rangeIndex)};

    for (; it; ++it) {
        auto nkey = parseNFTKey(it->key().ToString());
        if (to_pair<0, 1>(nkey) != std::make_pair(propertyId, rangeIndex)) {
            break;
        }
        tokenCount = std::max(tokenCount, std::max(std::get<2>(nkey), std::get<3>(nkey)));
    }
    return tokenCount;
}

/* Deletes a range of non-fungible tokens
 */
void CMPNonFungibleTokensDB::DeleteRange(const uint32_t &propertyId, const int64_t &tokenIdStart, const int64_t &tokenIdEnd, const NonFungibleStorage type)
{
    assert(pdb);
    std::string key = createNFTKey(propertyId, type, tokenIdStart, tokenIdEnd);
    pdb->Delete(leveldb::WriteOptions(), key);

    if (msc_debug_nftdb) PrintToLog("%s():%s, line %d, file: %s\n", __FUNCTION__, key, __LINE__, __FILE__);
}

/* Adds a range of non-fungible tokens and/or sets data on that range
 */
void CMPNonFungibleTokensDB::AddRange(const uint32_t &propertyId, const int64_t &tokenIdStart, const int64_t &tokenIdEnd, const std::string &info, const NonFungibleStorage type)
{
    assert(pdb);

    std::string key = createNFTKey(propertyId, type, tokenIdStart, tokenIdEnd);
    leveldb::Status status = pdb->Put(writeoptions, key, info);
    ++nWritten;

    if (msc_debug_nftdb) PrintToLog("%s():%s=%s:%s, line %d, file: %s\n", __FUNCTION__, key, info, status.ToString(), __LINE__, __FILE__);
}

/* Creates a range of non-fungible tokens
 */
std::pair<int64_t,int64_t> CMPNonFungibleTokensDB::CreateNonFungibleTokens(const uint32_t &propertyId, const int64_t &amount, const std::string &owner, const std::string &info)
{
    if (msc_debug_nftdb) PrintToLog("%s(): %d:%d:%s, line %d, file: %s\n", __FUNCTION__, propertyId, amount, owner, __LINE__, __FILE__);

    // negative amount will result in incorrect work
    if (amount < 0) {
        return {};
    }

    int64_t highestId = GetHighestRangeEnd(propertyId);
    int64_t newTokenStartId = highestId + 1;
    int64_t newTokenEndId = 0;

    if ( (highestId > (std::numeric_limits<int64_t>::max() - amount))) { /* overflow */
        newTokenEndId = std::numeric_limits<int64_t>::max();
    } else {
        newTokenEndId = highestId + amount;
    }

    AddRange(propertyId, newTokenStartId, newTokenEndId, info, NonFungibleStorage::GrantData);

    std::pair<int64_t,int64_t> newRange = std::make_pair(newTokenStartId, newTokenEndId);

    std::string highestRangeOwner = GetNonFungibleTokenValue(propertyId, highestId, NonFungibleStorage::RangeIndex);
    if (highestRangeOwner == owner) {
        std::pair<int64_t,int64_t> oldRange = GetRange(propertyId, highestId, NonFungibleStorage::RangeIndex);
        DeleteRange(propertyId, oldRange.first, oldRange.second, NonFungibleStorage::RangeIndex);
        newTokenStartId = oldRange.first; // override range start to merge ranges from same owner
    }

    AddRange(propertyId, newTokenStartId, newTokenEndId, owner, NonFungibleStorage::RangeIndex);

    return newRange;
}

/* Gets the info set in a non-fungible token
 */
std::string CMPNonFungibleTokensDB::GetNonFungibleTokenValue(const uint32_t &propertyId, const int64_t &tokenId, const NonFungibleStorage type)
{
    assert(pdb);
    CDBaseIterator it{NewIterator(), createNFTKey(propertyId, type)};

    for (; it; ++it) {
        auto nkey = parseNFTKey(it->key().ToString());
        if (to_pair<0, 1>(nkey) != std::make_pair(propertyId, type)) {
            break;
        }
        if (tokenId >= std::get<2>(nkey) && tokenId <= std::get<3>(nkey)) {
            return it->value().ToString();
        }
    }

    return {}; // not found
}

/* Gets the ranges of non-fungible tokens owned by an address
 */
std::map<uint32_t, std::vector<std::pair<int64_t, int64_t>>> CMPNonFungibleTokensDB::GetAddressNonFungibleTokens(const uint32_t &propertyId, const std::string &address)
{
    std::map<uint32_t, std::vector<std::pair<int64_t, int64_t>>> uniqueMap;
    assert(pdb);
    auto rangeIndex = NonFungibleStorage::RangeIndex;
    CDBaseIterator it{NewIterator(), createNFTKey(propertyId, rangeIndex)};

    for (; it; ++it) {
        auto nkey = parseNFTKey(it->key().ToString());
        if (propertyId != 0 && to_pair<0, 1>(nkey) != std::make_pair(propertyId, rangeIndex)) {
            break;
        }
        if (std::get<1>(nkey) != rangeIndex) {
            continue;
        }
        std::string value = it->value().ToString();
        if (value != address) continue;

        uniqueMap[std::get<0>(nkey)].emplace_back(std::get<2>(nkey), std::get<3>(nkey));
    }

    return uniqueMap;
}

/* Gets the ranges of non-fungible tokens for a property
 */
std::vector<std::pair<std::string,std::pair<int64_t,int64_t>>> CMPNonFungibleTokensDB::GetNonFungibleTokenRanges(const uint32_t &propertyId)
{
    std::vector<std::pair<std::string,std::pair<int64_t,int64_t>>> rangeMap;

    assert(pdb);
    auto rangeIndex = NonFungibleStorage::RangeIndex;
    CDBaseIterator it{NewIterator(), createNFTKey(propertyId, rangeIndex)};

    for (; it; ++it) {
        auto nkey = parseNFTKey(it->key().ToString());
        if (to_pair<0, 1>(nkey) != std::make_pair(propertyId, rangeIndex)) {
            break;
        }
        rangeMap.emplace_back(it->value().ToString(), to_pair<2, 3>(nkey));
    }

    return rangeMap;
}

void CMPNonFungibleTokensDB::SanityCheck()
{
    assert(pdb);

    std::string result;

    std::map<uint32_t,int64_t> totals;

    CDBaseIterator it{NewIterator()};
    for (; it; ++it) {
        auto nkey = parseNFTKey(it->key().ToString());
        if (std::get<1>(nkey) != NonFungibleStorage::RangeIndex) continue;

        auto& prop = totals[std::get<0>(nkey)];
        prop = std::max(prop, std::get<3>(nkey));
    }

    for (std::map<uint32_t,int64_t>::iterator it = totals.begin(); it != totals.end(); ++it) {
        if (mastercore::getTotalTokens(it->first) != it->second) {
            std::string abortMsg = strprintf("Failed sanity check on property %d (%d != %d)\n", it->first, mastercore::getTotalTokens(it->first), it->second);
            AbortNode(abortMsg);
        } else if (msc_debug_nftdb) {
            result += strprintf("%d:%d=%d,", it->first, mastercore::getTotalTokens(it->first), it->second);
        }
    }

    if (msc_debug_nftdb) PrintToLog("UTDB sanity check OK (%s)\n", result);
}

void CMPNonFungibleTokensDB::printStats()
{
    PrintToLog("CMPTxList stats: nWritten= %d , nRead= %d\n", nWritten, nRead);
}

void CMPNonFungibleTokensDB::printAll()
{
    int count = 0;
    CDBaseIterator it{NewIterator()};

    for(; it; ++it) {
        auto skey = it->key().ToString();
        auto svalue = it->value().ToString();
        ++count;
        PrintToConsole("entry #%8d= %s:%s\n", count, skey, svalue);
//      PrintToLog("entry #%8d= %s:%s\n", count, skey.ToString(), svalue.ToString());
    }
}

