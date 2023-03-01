/**
 * @file nftdb.cpp
 *
 * This file contains functionality for the non-fungible tokens database.
 */

#include <omnicore/nftdb.h>
#include <omnicore/omnicore.h>
#include <omnicore/errors.h>
#include <omnicore/log.h>

#include <unordered_map>
#include <utility>
#include <validation.h>

#include <climits>
#include <cstdint>
#include <stdint.h>
#include <tuple>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

typedef std::underlying_type<NonFungibleStorage>::type StorageType;

struct NFTKey {
    static constexpr uint8_t prefix = 'A';
    uint32_t propertyId;
    NonFungibleStorage type;
    int64_t tokenIdStart;
    int64_t tokenIdEnd;

    template<typename Stream>
    void Serialize(Stream& s) const
    {
        ser_writedata32be(s, propertyId);
        ser_writedata8(s, static_cast<StorageType>(type));
        ser_writedata64(s, tokenIdStart);
        ser_writedata64(s, tokenIdEnd);
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
        propertyId = ser_readdata32be(s);
        type = static_cast<NonFungibleStorage>(ser_readdata8(s));
        tokenIdStart = ser_readdata64(s);
        tokenIdEnd = ser_readdata64(s);
    }

    std::string ToString() const
    {
        return strprintf("%010d_%u_%020d-%020d", propertyId, static_cast<StorageType>(type), tokenIdStart, tokenIdEnd);
    }
};

constexpr uint8_t NFTKey::prefix;

inline bool Equal(const NFTKey& key, uint32_t propertyId, NonFungibleStorage type)
{
    return std::make_pair(key.propertyId, key.type) == std::make_pair(propertyId, type);
}

/* Gets the range a non-fungible token is in
 */
std::pair<int64_t,int64_t> CMPNonFungibleTokensDB::GetRange(const uint32_t &propertyId, const int64_t &tokenId, const NonFungibleStorage type)
{
    CDBaseIterator it{NewIterator(), NFTKey{propertyId, type}};

    for (; it; ++it) {
        auto nkey = it.Key<NFTKey>();
        if (!Equal(nkey, propertyId, type)) {
            break;
        }
        if (tokenId >= nkey.tokenIdStart && tokenId <= nkey.tokenIdEnd) {
            return {nkey.tokenIdStart, nkey.tokenIdEnd};
        }
    }

    return std::make_pair(0, 0); // token not found, return zero'd range
}

/* Checks if the range of tokens is contiguous (ie owned by a single address)
 */
std::string CMPNonFungibleTokensDB::GetNonFungibleTokenValueInRange(const uint32_t &propertyId, const int64_t &rangeStart, const int64_t &rangeEnd)
{
    auto rangeIndex = NonFungibleStorage::RangeIndex;
    CDBaseIterator it{NewIterator(), NFTKey{propertyId, rangeIndex}};
    for (; it; ++it) {
        auto nkey = it.Key<NFTKey>();
        if (!Equal(nkey, propertyId, rangeIndex)) {
            break;
        }
        if (rangeStart >= nkey.tokenIdStart && rangeEnd <= nkey.tokenIdEnd) {
            return it.Value().ToString();
        }
    }

    return {}; // range doesn't exist
}

/* Moves a range of tokens (returns false if not able to move)
 */
bool CMPNonFungibleTokensDB::MoveNonFungibleTokens(const uint32_t &propertyId, const int64_t &tokenIdStart, const int64_t &tokenIdEnd, const std::string &from, const std::string &to)
{
    if (msc_debug_nftdb) PrintToLog("%s(): %d:%d:%d:%s:%s, line %d, file: %s\n", __FUNCTION__, propertyId, tokenIdStart, tokenIdEnd, from, to, __LINE__, __FILE__);

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
    int64_t tokenCount = 0;
    auto rangeIndex = NonFungibleStorage::RangeIndex;
    CDBaseIterator it{NewIterator(), NFTKey{propertyId, rangeIndex}};

    for (; it; ++it) {
        auto nkey = it.Key<NFTKey>();
        if (!Equal(nkey, propertyId, rangeIndex)) {
            break;
        }
        tokenCount = std::max(tokenCount, std::max(nkey.tokenIdStart, nkey.tokenIdEnd));
    }
    return tokenCount;
}

struct DBHeightKey {
    static constexpr uint8_t prefix = 'H';
    int height;

    template<typename Stream>
    void Serialize(Stream& s) const
    {
        ser_writedata32be(s, height);
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
        height = ser_readdata32be(s);
    }
};

constexpr uint8_t DBHeightKey::prefix;

struct DBRollbackValue {
    std::unordered_map<std::string, CRollbackData>& changes;

    template<typename Stream>
    void Serialize(Stream& s) const
    {
        for (auto& it : changes) {
            s << it.first << it.second.type << it.second.data;
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
        std::string dbkey;
        CRollbackData dbvalue;
        while (!s.empty()) {
            s >> dbkey >> dbvalue.type >> dbvalue.data;
            // insert key if not present
            changes.emplace(dbkey, dbvalue);
        }
    }
};

void CMPNonFungibleTokensDB::WriteBlockCache(int height, bool sanityCheck)
{
    if (!blockData.empty()) {
        if (sanityCheck)
            SanityCheck();
        Write(DBHeightKey{height}, DBRollbackValue{blockData});
        blockData.clear();
    }
}

void CMPNonFungibleTokensDB::RollBackAboveBlock(int height)
{
    leveldb::WriteBatch batch;
    std::unordered_map<std::string, CRollbackData> changes;
    CDBaseIterator it{NewIterator(), DBHeightKey{height}};
    for (; it; ++it) {
        // erase rollback key
        batch.Delete(it.Key());
        it.Value(DBRollbackValue{changes});
    }
    // unique keys to update, it moves container to batch
    for (auto it = changes.begin(); it != changes.end();) {
        auto m_it = std::make_move_iterator(it++);
        if (m_it->second.type == CRollbackData::DELETE_KEY) {
            batch.Delete(m_it->first);
        } else {
            batch.Put(m_it->first, m_it->second.data);
        }
    }
    assert(pdb);
    pdb->Write(writeoptions, &batch);
}

void CMPNonFungibleTokensDB::StoreBlockCache(const std::string& key)
{
    if (blockData.find(key) == blockData.end()) {
        CRollbackData rollback{CRollbackData::PERSIST_KEY};
        leveldb::Status status = pdb->Get(readoptions, key, &rollback.data);
        if (status.IsNotFound()) {
            rollback.type = CRollbackData::DELETE_KEY;
        }
        blockData.emplace(key, std::move(rollback));
    }
}

/* Deletes a range of non-fungible tokens
 */
void CMPNonFungibleTokensDB::DeleteRange(const uint32_t &propertyId, const int64_t &tokenIdStart, const int64_t &tokenIdEnd, const NonFungibleStorage type)
{
    NFTKey key{propertyId, type, tokenIdStart, tokenIdEnd};
    StoreBlockCache(KeyToString(key));
    Delete(key);

    if (msc_debug_nftdb) PrintToLog("%s():%s, line %d, file: %s\n", __FUNCTION__, key.ToString(), __LINE__, __FILE__);
}

/* Adds a range of non-fungible tokens and/or sets data on that range
 */
void CMPNonFungibleTokensDB::AddRange(const uint32_t &propertyId, const int64_t &tokenIdStart, const int64_t &tokenIdEnd, const std::string &info, const NonFungibleStorage type)
{
    NFTKey key{propertyId, type, tokenIdStart, tokenIdEnd};
    StoreBlockCache(KeyToString(key));
    auto status = Write(key, info);
    ++nWritten;

    if (msc_debug_nftdb) PrintToLog("%s():%s=%s:%s, line %d, file: %s\n", __FUNCTION__, key.ToString(), info, (status ? "OK" : "Error"), __LINE__, __FILE__);
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
    CDBaseIterator it{NewIterator(), NFTKey{propertyId, type}};

    for (; it; ++it) {
        auto nkey = it.Key<NFTKey>();
        if (!Equal(nkey, propertyId, type)) {
            break;
        }
        if (tokenId >= nkey.tokenIdStart && tokenId <= nkey.tokenIdEnd) {
            return it.Value().ToString();
        }
    }

    return {}; // not found
}

/* Gets the ranges of non-fungible tokens owned by an address
 */
std::map<uint32_t, std::vector<std::pair<int64_t, int64_t>>> CMPNonFungibleTokensDB::GetAddressNonFungibleTokens(const uint32_t &propertyId, const std::string &address)
{
    std::map<uint32_t, std::vector<std::pair<int64_t, int64_t>>> uniqueMap;

    auto rangeIndex = NonFungibleStorage::RangeIndex;
    CDBaseIterator it{NewIterator(), NFTKey{propertyId, rangeIndex}};

    for (; it; ++it) {
        auto nkey = it.Key<NFTKey>();
        if (propertyId != 0 && !Equal(nkey, propertyId, rangeIndex)) {
            break;
        }
        if (nkey.type != rangeIndex) {
            continue;
        }
        if (it.Value() != address) continue;

        uniqueMap[nkey.propertyId].emplace_back(nkey.tokenIdStart, nkey.tokenIdEnd);
    }

    return uniqueMap;
}

/* Gets the ranges of non-fungible tokens for a property
 */
std::vector<std::pair<std::string,std::pair<int64_t,int64_t>>> CMPNonFungibleTokensDB::GetNonFungibleTokenRanges(const uint32_t &propertyId)
{
    std::vector<std::pair<std::string,std::pair<int64_t,int64_t>>> rangeMap;

    auto rangeIndex = NonFungibleStorage::RangeIndex;
    CDBaseIterator it{NewIterator(), NFTKey{propertyId, rangeIndex}};

    for (; it; ++it) {
        auto nkey = it.Key<NFTKey>();
        if (!Equal(nkey, propertyId, rangeIndex)) {
            break;
        }
        rangeMap.emplace_back(it.Value().ToString(), std::make_pair(nkey.tokenIdStart, nkey.tokenIdEnd));
    }

    return rangeMap;
}

void CMPNonFungibleTokensDB::SanityCheck()
{
    NFTKey key;
    std::string result;
    std::map<uint32_t, int64_t> totals;

    // check only keys that are changed in a block
    for (auto& data : blockData) {
        assert(StringToKey(data.first, key));
        if (key.type != NonFungibleStorage::RangeIndex) continue;
        if (totals.count(key.propertyId)) continue;
        CDBaseIterator it{NewIterator(), NFTKey{key.propertyId, key.type}};
        for (; it; ++it) {
            auto nkey = it.Key<NFTKey>();
            if (!Equal(nkey, key.propertyId, key.type)) {
                break;
            }
            auto& prop = totals[nkey.propertyId];
            prop = std::max(prop, nkey.tokenIdEnd);
        }
    }

    for (std::map<uint32_t,int64_t>::iterator it = totals.begin(); it != totals.end(); ++it) {
        auto total = mastercore::getTotalTokens(it->first);
        if (total != it->second) {
            AbortNode(strprintf("Failed sanity check on property %d (%d != %d)\n", it->first, total, it->second));
        } else if (msc_debug_nftdb) {
            result += strprintf("%d:%d=%d,", it->first, total, it->second);
        }
    }

    if (msc_debug_nftdb && !result.empty()) PrintToLog("UTDB sanity check OK (%s)\n", result);
}

void CMPNonFungibleTokensDB::printStats()
{
    PrintToLog("CMPTxList stats: nWritten= %d , nRead= %d\n", nWritten, nRead);
}

void CMPNonFungibleTokensDB::printAll()
{
    int count = 0;
    CDBaseIterator it{NewIterator(), NFTKey{0}};

    for(; it; ++it) {
        auto skey = it.Key<NFTKey>();
        auto svalue = it.Value();
        ++count;
        PrintToConsole("entry #%8d= %s:%s\n", count, skey.ToString(), svalue.ToString());
//      PrintToLog("entry #%8d= %s:%s\n", count, skey.ToString(), svalue.ToString());
    }
}

