#include <omnicore/dbspinfo.h>

#include <omnicore/dbbase.h>
#include <omnicore/log.h>

#include <base58.h>
#include <clientversion.h>
#include <fs.h>
#include <key_io.h>
#include <serialize.h>
#include <streams.h>
#include <tinyformat.h>
#include <uint256.h>

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

#include <boost/lexical_cast.hpp>

#include <stdint.h>

#include <string>


CMPSPInfo::Entry::Entry()
  : prop_type(0), prev_prop_id(0), num_tokens(0), property_desired(0),
    deadline(0), early_bird(0), percentage(0),
    close_early(false), max_tokens(false), missedTokens(0), timeclosed(0),
    fixed(false), manual(false), unique(false), delegate("") {}

bool CMPSPInfo::Entry::isDivisible() const
{
    switch (prop_type) {
        case MSC_PROPERTY_TYPE_DIVISIBLE:
        case MSC_PROPERTY_TYPE_DIVISIBLE_REPLACING:
        case MSC_PROPERTY_TYPE_DIVISIBLE_APPENDING:
            return true;
    }
    return false;
}

void CMPSPInfo::Entry::print() const
{
    PrintToConsole("%s:%s(Fixed=%s,Divisible=%s):%d:%s/%s, %s %s\n",
            issuer,
            name,
            fixed ? "Yes" : "No",
            manual ? "Yes" : "No",
            unique ? "Yes" : "No",
            isDivisible() ? "Yes" : "No",
            num_tokens,
            category, subcategory, url, data);
}

/**
 * Stores a new issuer in the DB.
 *
 * @param block  The block of the update
 * @param idx    The position within the block of the update
 * @param newIssuer  The new issuer
 */
void CMPSPInfo::Entry::updateIssuer(int block, int idx, const std::string& newIssuer)
{
    historicalIssuers[std::make_pair(block, idx)] = newIssuer;
}

/**
 * Returns the issuer for the given block.
 *
 * @param block  The block to check
 * @return The issuer of that block
 */
std::string CMPSPInfo::Entry::getIssuer(int block) const
{
    std::string _issuer = issuer;

    for (auto const& entry : historicalIssuers) {
        int currentBlock = entry.first.first;
        std::string currentIssuer = entry.second;

        if (currentBlock > block) {
            break;
        }

        _issuer = currentIssuer;
    }

    return _issuer;
}

/**
 * Stores a new delegate in the DB.
 *
 * @param block  The block of the update
 * @param idx    The position within the block of the update
 * @param newIssuer  The new delegate
 */
void CMPSPInfo::Entry::addDelegate(int block, int idx, const std::string& newIssuer)
{
    historicalDelegates[std::make_pair(block, idx)] = newIssuer;
}
   
/**
 * Clears the delegate in the DB.
 *
 * @param block  The block of the update
 * @param idx    The position within the block of the update
 */
void CMPSPInfo::Entry::removeDelegate(int block, int idx)
{
    historicalDelegates[std::make_pair(block, idx)] = "";
}

/**
 * Returns the delegate for the given block, if there is one.
 * If not, return an emptry string.
 *
 * @param block  The block to check
 * @return The issuer of that block
 */
std::string CMPSPInfo::Entry::getDelegate(int block) const
{
    std::string _delegate = delegate;

    for (auto const& entry : historicalDelegates) {
        int currentBlock = entry.first.first;
        std::string currentDelegate = entry.second;

        if (currentBlock > block) {
            break;
        }

        _delegate = currentDelegate;
    }

    return _delegate;
}


CMPSPInfo::CMPSPInfo(const fs::path& path, bool fWipe)
{
    leveldb::Status status = Open(path, fWipe);
    PrintToConsole("Loading smart property database: %s\n", status.ToString());

    // special cases for constant SPs OMN and TOMN
    implied_omni.issuer = EncodeDestination(ExodusAddress());
    implied_omni.updateIssuer(0, 0, implied_omni.issuer);
    implied_omni.prop_type = MSC_PROPERTY_TYPE_DIVISIBLE;
    implied_omni.num_tokens = 700000;
    implied_omni.category = "N/A";
    implied_omni.subcategory = "N/A";
    implied_omni.name = "OmniXEP tokens";
    implied_omni.url = "https://www.electraprotocol.com/g";
    implied_omni.data = "Omni tokens serve as the binding between Xep, smart properties and contracts created on the Omni Layer.";

    implied_tomni.issuer = EncodeDestination(ExodusAddress());
    implied_tomni.updateIssuer(0, 0, implied_tomni.issuer);
    implied_tomni.prop_type = MSC_PROPERTY_TYPE_DIVISIBLE;
    implied_tomni.num_tokens = 700000;
    implied_tomni.category = "N/A";
    implied_tomni.subcategory = "N/A";
    implied_tomni.name = "Test OmniXEP tokens";
    implied_tomni.url = "https://www.electraprotocol.com/";
    implied_tomni.data = "Test Omni tokens serve as the binding between Xep, smart properties and contracts created on the Omni Layer.";

    init();
}

CMPSPInfo::~CMPSPInfo()
{
    if (msc_debug_persistence) PrintToLog("CMPSPInfo closed\n");
}

void CMPSPInfo::Clear()
{
    // wipe database via parent class
    CDBBase::Clear();
    // reset "next property identifiers"
    init();
}

void CMPSPInfo::init(uint32_t nextSPID, uint32_t nextTestSPID)
{
    next_spid = nextSPID;
    next_test_spid = nextTestSPID;
}

uint32_t CMPSPInfo::peekNextSPID(uint8_t ecosystem) const
{
    uint32_t nextId = 0;

    switch (ecosystem) {
        case OMNI_PROPERTY_MSC: // Main ecosystem, MSC: 1, TMSC: 2, First available SP = 3
            nextId = next_spid;
            break;
        case OMNI_PROPERTY_TMSC: // Test ecosystem, same as above with high bit set
            nextId = next_test_spid;
            break;
        default: // Non-standard ecosystem, ID's start at 0
            nextId = 0;
    }

    return nextId;
}

bool CMPSPInfo::updateSP(uint32_t propertyId, const Entry& info)
{
    // cannot update implied SP
    if (OMNI_PROPERTY_MSC == propertyId || OMNI_PROPERTY_TMSC == propertyId) {
        return false;
    }

    // DB key for property entry
    CDataStream ssSpKey(SER_DISK, CLIENT_VERSION);
    ssSpKey << std::make_pair('s', propertyId);
    leveldb::Slice slSpKey(&ssSpKey[0], ssSpKey.size());

    // DB value for property entry
    CDataStream ssSpValue(SER_DISK, CLIENT_VERSION);
    ssSpValue.reserve(::GetSerializeSize(info, CLIENT_VERSION));
    ssSpValue << info;
    leveldb::Slice slSpValue(&ssSpValue[0], ssSpValue.size());

    // DB key for historical property entry
    CDataStream ssSpPrevKey(SER_DISK, CLIENT_VERSION);
    ssSpPrevKey << 'b';
    ssSpPrevKey << info.update_block;
    ssSpPrevKey << propertyId;
    leveldb::Slice slSpPrevKey(&ssSpPrevKey[0], ssSpPrevKey.size());

    leveldb::WriteBatch batch;
    std::string strSpPrevValue;

    // if a value exists move it to the old key
    if (!pdb->Get(readoptions, slSpKey, &strSpPrevValue).IsNotFound()) {
        batch.Put(slSpPrevKey, strSpPrevValue);
    }
    batch.Put(slSpKey, slSpValue);

    // Update delegate info if set
    if (!info.historicalDelegates.empty()) {
        std::string delegateKey = strprintf("DE-%d", propertyId);
        CDataStream ssDelegateValue(SER_DISK, CLIENT_VERSION);
        ssDelegateValue << info.delegate;
        ssDelegateValue << info.historicalDelegates;
        leveldb::Slice slDelegateValue(&ssDelegateValue[0], ssDelegateValue.size());
        batch.Put(delegateKey, slDelegateValue);
    }

    leveldb::Status status = pdb->Write(syncoptions, &batch);

    if (!status.ok()) {
        PrintToLog("%s(): ERROR for SP %d: %s\n", __func__, propertyId, status.ToString());
        return false;
    }

    PrintToLog("%s(): updated entry for SP %d successfully\n", __func__, propertyId);
    return true;
}


uint32_t CMPSPInfo::putSP(uint8_t ecosystem, const Entry& info)
{
    uint32_t propertyId = 0;
    switch (ecosystem) {
        case OMNI_PROPERTY_MSC: // Main ecosystem, MSC: 1, TMSC: 2, First available SP = 3
            propertyId = next_spid++;
            break;
        case OMNI_PROPERTY_TMSC: // Test ecosystem, same as above with high bit set
            propertyId = next_test_spid++;
            break;
        default: // Non-standard ecosystem, ID's start at 0
            propertyId = 0;
    }

    // DB key for property entry
    CDataStream ssSpKey(SER_DISK, CLIENT_VERSION);
    ssSpKey << std::make_pair('s', propertyId);
    leveldb::Slice slSpKey(&ssSpKey[0], ssSpKey.size());

    // DB value for property entry
    CDataStream ssSpValue(SER_DISK, CLIENT_VERSION);
    ssSpValue.reserve(::GetSerializeSize(info, CLIENT_VERSION));
    ssSpValue << info;
    leveldb::Slice slSpValue(&ssSpValue[0], ssSpValue.size());

    // DB key for identifier lookup entry
    CDataStream ssTxIndexKey(SER_DISK, CLIENT_VERSION);
    ssTxIndexKey << std::make_pair('t', info.txid);
    leveldb::Slice slTxIndexKey(&ssTxIndexKey[0], ssTxIndexKey.size());

    // DB value for identifier
    CDataStream ssTxValue(SER_DISK, CLIENT_VERSION);
    ssTxValue.reserve(::GetSerializeSize(propertyId, CLIENT_VERSION));
    ssTxValue << propertyId;
    leveldb::Slice slTxValue(&ssTxValue[0], ssTxValue.size());

    // sanity checking
    std::string existingEntry;
    if (!pdb->Get(readoptions, slSpKey, &existingEntry).IsNotFound() && slSpValue.compare(existingEntry) != 0) {
        std::string strError = strprintf("writing SP %d to DB, when a different SP already exists for that identifier", propertyId);
        PrintToLog("%s() ERROR: %s\n", __func__, strError);
    } else if (!pdb->Get(readoptions, slTxIndexKey, &existingEntry).IsNotFound() && slTxValue.compare(existingEntry) != 0) {
        std::string strError = strprintf("writing index txid %s : SP %d is overwriting a different value", info.txid.ToString(), propertyId);
        PrintToLog("%s() ERROR: %s\n", __func__, strError);
    }

    // Create and check unique field
    std::string uniqueKey = strprintf("UE-%d", propertyId);
    if (info.unique) {
        // sanity checking
        if (!pdb->Get(readoptions, uniqueKey, &existingEntry).IsNotFound() && existingEntry != strprintf("%d", info.unique)) {
            std::string strError = strprintf("writing SP %d unique field to DB, when a different SP already exists for that identifier", propertyId);
            PrintToLog("%s() ERROR: %s\n", __func__, strError);
        }
    }

    // atomically write both the the SP and the index to the database
    leveldb::WriteBatch batch;
    batch.Put(slSpKey, slSpValue);
    batch.Put(slTxIndexKey, slTxValue);

    if (info.unique) {
        batch.Put(uniqueKey, strprintf("%d", info.unique));
    }

    leveldb::Status status = pdb->Write(syncoptions, &batch);

    if (!status.ok()) {
        PrintToLog("%s(): ERROR for SP %d: %s\n", __func__, propertyId, status.ToString());
    }

    return propertyId;
}

bool CMPSPInfo::getSP(uint32_t propertyId, Entry& info) const
{
    // special cases for constant SPs MSC and TMSC
    if (OMNI_PROPERTY_MSC == propertyId) {
        info = implied_omni;
        return true;
    } else if (OMNI_PROPERTY_TMSC == propertyId) {
        info = implied_tomni;
        return true;
    }

    // DB key for property entry
    CDataStream ssSpKey(SER_DISK, CLIENT_VERSION);
    ssSpKey << std::make_pair('s', propertyId);
    leveldb::Slice slSpKey(&ssSpKey[0], ssSpKey.size());

    // DB value for property entry
    std::string strSpValue;
    leveldb::Status status = pdb->Get(readoptions, slSpKey, &strSpValue);
    if (!status.ok()) {
        if (!status.IsNotFound()) {
            PrintToLog("%s(): ERROR for SP %d: %s\n", __func__, propertyId, status.ToString());
        }
        return false;
    }

    try {
        CDataStream ssSpValue(strSpValue.data(), strSpValue.data() + strSpValue.size(), SER_DISK, CLIENT_VERSION);
        ssSpValue >> info;
    } catch (const std::exception& e) {
        PrintToLog("%s(): ERROR for SP %d: %s\n", __func__, propertyId, e.what());
        return false;
    }

    // Check for unique entry
    std::string uniqueKey = strprintf("UE-%d", propertyId);
    std::string uniqueValue;
    leveldb::Status statusUnique = pdb->Get(readoptions, uniqueKey, &uniqueValue);
    if (statusUnique.ok() && !statusUnique.IsNotFound()) {
        try {
            info.unique = boost::lexical_cast<bool>(uniqueValue);
        } catch (boost::bad_lexical_cast const &e) {
            PrintToLog("%s(): ERROR for SP unique field %d: %s\n", __func__, propertyId, e.what());
            return false;
        }
    }

    // Check for delegate entry
    std::string delegateKey = strprintf("DE-%d", propertyId);
    std::string delegateValue;
    leveldb::Status statusDelegate = pdb->Get(readoptions, delegateKey, &delegateValue);
    if (statusDelegate.ok() && !statusDelegate.IsNotFound()) {
        try {
            CDataStream ssDelegateValue(delegateValue.data(), delegateValue.data() + delegateValue.size(), SER_DISK, CLIENT_VERSION);
            ssDelegateValue >> info.delegate;
            ssDelegateValue >> info.historicalDelegates;
        } catch (const std::exception& e) {
            PrintToLog("%s(): ERROR for SP delegates %d: %s\n", __func__, propertyId, e.what());
            return false;
        }
    }

    return true;
}

bool CMPSPInfo::hasSP(uint32_t propertyId) const
{
    // Special cases for constant SPs MSC and TMSC
    if (OMNI_PROPERTY_MSC == propertyId || OMNI_PROPERTY_TMSC == propertyId) {
        return true;
    }

    // DB key for property entry
    CDataStream ssSpKey(SER_DISK, CLIENT_VERSION);
    ssSpKey << std::make_pair('s', propertyId);
    leveldb::Slice slSpKey(&ssSpKey[0], ssSpKey.size());

    // DB value for property entry
    std::string strSpValue;
    leveldb::Status status = pdb->Get(readoptions, slSpKey, &strSpValue);

    return status.ok();
}

uint32_t CMPSPInfo::findSPByTX(const uint256& txid) const
{
    uint32_t propertyId = 0;

    // DB key for identifier lookup entry
    CDataStream ssTxIndexKey(SER_DISK, CLIENT_VERSION);
    ssTxIndexKey << std::make_pair('t', txid);
    leveldb::Slice slTxIndexKey(&ssTxIndexKey[0], ssTxIndexKey.size());

    // DB value for identifier
    std::string strTxIndexValue;
    if (!pdb->Get(readoptions, slTxIndexKey, &strTxIndexValue).ok()) {
        std::string strError = strprintf("failed to find property created with %s", txid.GetHex());
        PrintToLog("%s(): ERROR: %s", __func__, strError);
        return 0;
    }

    try {
        CDataStream ssValue(strTxIndexValue.data(), strTxIndexValue.data() + strTxIndexValue.size(), SER_DISK, CLIENT_VERSION);
        ssValue >> propertyId;
    } catch (const std::exception& e) {
        PrintToLog("%s(): ERROR: %s\n", __func__, e.what());
        return 0;
    }

    return propertyId;
}

int64_t CMPSPInfo::popBlock(const uint256& block_hash)
{
    int64_t remainingSPs = 0;
    leveldb::WriteBatch commitBatch;
    leveldb::Iterator* iter = NewIterator();

    CDataStream ssSpKeyPrefix(SER_DISK, CLIENT_VERSION);
    ssSpKeyPrefix << 's';
    leveldb::Slice slSpKeyPrefix(&ssSpKeyPrefix[0], ssSpKeyPrefix.size());

    for (iter->Seek(slSpKeyPrefix); iter->Valid() && iter->key().starts_with(slSpKeyPrefix); iter->Next()) {
        // deserialize the persisted value
        leveldb::Slice slSpValue = iter->value();
        Entry info;
        try {
            CDataStream ssValue(slSpValue.data(), slSpValue.data() + slSpValue.size(), SER_DISK, CLIENT_VERSION);
            ssValue >> info;
        } catch (const std::exception& e) {
            PrintToLog("%s(): ERROR: %s\n", __func__, e.what());
            return -1;
        }
        // pop the block
        if (info.update_block == block_hash) {
            leveldb::Slice slSpKey = iter->key();

            // need to roll this SP back
            if (info.update_block == info.creation_block) {
                // this is the block that created this SP, so delete the SP and the tx index entry
                CDataStream ssTxIndexKey(SER_DISK, CLIENT_VERSION);
                ssTxIndexKey << std::make_pair('t', info.txid);
                leveldb::Slice slTxIndexKey(&ssTxIndexKey[0], ssTxIndexKey.size());
                commitBatch.Delete(slSpKey);
                commitBatch.Delete(slTxIndexKey);
            } else {
                uint32_t propertyId = 0;
                try {
                    CDataStream ssValue(1+slSpKey.data(), 1+slSpKey.data()+slSpKey.size(), SER_DISK, CLIENT_VERSION);
                    ssValue >> propertyId;
                } catch (const std::exception& e) {
                    PrintToLog("%s(): ERROR: %s\n", __func__, e.what());
                    return -2;
                }

                CDataStream ssSpPrevKey(SER_DISK, CLIENT_VERSION);
                ssSpPrevKey << 'b';
                ssSpPrevKey << info.update_block;
                ssSpPrevKey << propertyId;
                leveldb::Slice slSpPrevKey(&ssSpPrevKey[0], ssSpPrevKey.size());

                std::string strSpPrevValue;
                if (!pdb->Get(readoptions, slSpPrevKey, &strSpPrevValue).IsNotFound()) {
                    // copy the prev state to the current state and delete the old state
                    commitBatch.Put(slSpKey, strSpPrevValue);
                    commitBatch.Delete(slSpPrevKey);
                    ++remainingSPs;
                } else {
                    // failed to find a previous SP entry, trigger reparse
                    PrintToLog("%s(): ERROR: failed to retrieve previous SP entry\n", __func__);
                    return -3;
                }
            }
        } else {
            ++remainingSPs;
        }
    }

    // clean up the iterator
    delete iter;

    leveldb::Status status = pdb->Write(syncoptions, &commitBatch);

    if (!status.ok()) {
        PrintToLog("%s(): ERROR: %s\n", __func__, status.ToString());
        return -4;
    }

    return remainingSPs;
}

void CMPSPInfo::setWatermark(const uint256& watermark)
{
    leveldb::WriteBatch batch;

    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey << 'B';
    leveldb::Slice slKey(&ssKey[0], ssKey.size());

    CDataStream ssValue(SER_DISK, CLIENT_VERSION);
    ssValue.reserve(::GetSerializeSize(watermark, CLIENT_VERSION));
    ssValue << watermark;
    leveldb::Slice slValue(&ssValue[0], ssValue.size());

    batch.Delete(slKey);
    batch.Put(slKey, slValue);

    leveldb::Status status = pdb->Write(syncoptions, &batch);
    if (!status.ok()) {
        PrintToLog("%s(): ERROR: failed to write watermark: %s\n", __func__, status.ToString());
    }
}

bool CMPSPInfo::getWatermark(uint256& watermark) const
{
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey << 'B';
    leveldb::Slice slKey(&ssKey[0], ssKey.size());

    std::string strValue;
    leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
    if (!status.ok()) {
        if (!status.IsNotFound()) {
            PrintToLog("%s(): ERROR: failed to retrieve watermark: %s\n", __func__, status.ToString());
        }
        return false;
    }

    try {
        CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(), SER_DISK, CLIENT_VERSION);
        ssValue >> watermark;
    } catch (const std::exception& e) {
        PrintToLog("%s(): ERROR: failed to deserialize watermark: %s\n", __func__, e.what());
        return false;
    }

    return true;
}

void CMPSPInfo::printAll() const
{
    // print off the hard coded MSC and TMSC entries
    for (uint32_t idx = OMNI_PROPERTY_MSC; idx <= OMNI_PROPERTY_TMSC; idx++) {
        Entry info;
        PrintToConsole("%10d => ", idx);
        if (getSP(idx, info)) {
            info.print();
        } else {
            PrintToConsole("<Internal Error on implicit SP>\n");
        }
    }

    leveldb::Iterator* iter = NewIterator();

    CDataStream ssSpKeyPrefix(SER_DISK, CLIENT_VERSION);
    ssSpKeyPrefix << 's';
    leveldb::Slice slSpKeyPrefix(&ssSpKeyPrefix[0], ssSpKeyPrefix.size());

    for (iter->Seek(slSpKeyPrefix); iter->Valid() && iter->key().starts_with(slSpKeyPrefix); iter->Next()) {
        leveldb::Slice slSpKey = iter->key();
        uint32_t propertyId = 0;
        try {
            CDataStream ssValue(1+slSpKey.data(), 1+slSpKey.data()+slSpKey.size(), SER_DISK, CLIENT_VERSION);
            ssValue >> propertyId;
        } catch (const std::exception& e) {
            PrintToLog("%s(): ERROR: %s\n", __func__, e.what());
            PrintToConsole("<Malformed key in DB>\n");
            continue;
        }
        PrintToConsole("%10s => ", propertyId);

        // deserialize the persisted data
        leveldb::Slice slSpValue = iter->value();
        Entry info;
        try {
            CDataStream ssSpValue(slSpValue.data(), slSpValue.data() + slSpValue.size(), SER_DISK, CLIENT_VERSION);
            ssSpValue >> info;
        } catch (const std::exception& e) {
            PrintToConsole("<Malformed value in DB>\n");
            PrintToLog("%s(): ERROR: %s\n", __func__, e.what());
            continue;
        }
        info.print();
    }

    // clean up the iterator
    delete iter;
}
