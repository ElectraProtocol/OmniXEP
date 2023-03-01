#ifndef XEP_OMNICORE_DBBASE_H
#define XEP_OMNICORE_DBBASE_H

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <leveldb/db.h>

#include <clientversion.h>
#include <fs.h>
#include <streams.h>

#include <assert.h>
#include <memory>
#include <stddef.h>

template<typename T>
bool StringToValue(std::string&& s, T& value)
{
    try {
        CDataStream ssValue(s.data(), s.data() + s.size(), SER_DISK, CLIENT_VERSION);
        ssValue >> value;
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

inline bool StringToValue(std::string&& s, std::string& value)
{
    value = std::move(s);
    return true;
}

template<typename T>
std::string ValueToString(const T& value)
{
    std::vector<uint8_t> v;
    CVectorWriter writer(SER_DISK, CLIENT_VERSION, v, 0);
    writer << value;
    return {v.begin(), v.end()};
}

inline std::string ValueToString(const std::string& value)
{
    return value;
}

/** Base class for LevelDB based storage.
 */
class CDBBase
{
private:
    //! Options used when iterating over values of the database
    leveldb::ReadOptions iteroptions;

protected:
    //! Database options used
    leveldb::Options options;

    //! Options used when reading from the database
    leveldb::ReadOptions readoptions;

    //! Options used when writing to the database
    leveldb::WriteOptions writeoptions;

    //! Options used when sync writing to the database
    leveldb::WriteOptions syncoptions;

    //! The database itself
    leveldb::DB* pdb;

    //! Number of entries read
    unsigned int nRead;

    //! Number of entries written
    unsigned int nWritten;

    CDBBase() : pdb(NULL), nRead(0), nWritten(0)
    {
        options.paranoid_checks = true;
        options.create_if_missing = true;
        options.compression = leveldb::kNoCompression;
        options.max_open_files = 64;
        readoptions.verify_checksums = true;
        iteroptions.verify_checksums = true;
        iteroptions.fill_cache = false;
        syncoptions.sync = true;
    }

    virtual ~CDBBase()
    {
        Close();
    }

    /**
     * Creates and returns a new LevelDB iterator.
     *
     * It is expected that the database is not closed. The iterator is owned by the
     * caller, and the object has to be deleted explicitly.
     *
     * @return A new LevelDB iterator
     */
    leveldb::Iterator* NewIterator() const
    {
        assert(pdb != NULL);
        return pdb->NewIterator(iteroptions);
    }

    template<typename K, typename V>
    bool Write(const K& key, const V& value)
    {
        static_assert(sizeof(K::prefix) == 1, "Prefix needs to be 1 byte");
        CDataStream ssKey(SER_DISK, CLIENT_VERSION, K::prefix, key);
        leveldb::Slice slKey(ssKey.data(), ssKey.size());
        assert(pdb);
        return pdb->Put(writeoptions, slKey, ValueToString(value)).ok();
    }

    template<typename K, typename V>
    bool Read(const K& key, V& value) const
    {
        static_assert(sizeof(K::prefix) == 1, "Prefix needs to be 1 byte");
        CDataStream ssKey(SER_DISK, CLIENT_VERSION, K::prefix, key);
        leveldb::Slice slKey(ssKey.data(), ssKey.size());
        std::string strValue;
        assert(pdb);
        return pdb->Get(readoptions, slKey, &strValue).ok()
            && StringToValue(std::move(strValue), value);
    }

    template<typename K>
    bool Delete(const K& key)
    {
        static_assert(sizeof(K::prefix) == 1, "Prefix needs to be 1 byte");
        CDataStream ssKey(SER_DISK, CLIENT_VERSION, K::prefix, key);
        leveldb::Slice slKey(ssKey.data(), ssKey.size());
        assert(pdb);
        return pdb->Delete(leveldb::WriteOptions(), slKey).ok();
    }

    /**
     * Opens or creates a LevelDB based database.
     *
     * If the database is wiped before opening, it's content is destroyed, including
     * all log files and meta data.
     *
     * @param path   The path of the database to open
     * @param fWipe  Whether to wipe the database before opening
     * @return A Status object, indicating success or failure
     */
    leveldb::Status Open(const fs::path& path, bool fWipe = false);

    /**
     * Deinitializes and closes the database.
     */
    void Close();

public:
    /**
     * Deletes all entries of the database, and resets the counters.
     */
    void Clear();
};

template<typename T>
std::string KeyToString(const T& key)
{
    std::vector<uint8_t> v;
    static_assert(sizeof(T::prefix) == 1, "Prefix needs to be 1 byte");
    CVectorWriter writer(SER_DISK, CLIENT_VERSION, v, 0);
    writer << T::prefix << key;
    return {v.begin(), v.end()};
}

template<typename T>
bool StringToKey(const std::string& s, T& key)
{
    auto prefix = T::prefix;
    static_assert(sizeof(T::prefix) == 1, "Prefix needs to be 1 byte");
    try {
        std::vector<uint8_t> v{s.begin(), s.end()};
        VectorReader reader(SER_DISK, CLIENT_VERSION, v, 0);
        reader >> prefix >> key;
    } catch (const std::exception&) {
        return false;
    }
    return prefix == T::prefix;
}

class CDBaseIterator
{
private:
    const uint8_t prefix = 0;
    const bool use_prefix = false;
    std::unique_ptr<leveldb::Iterator> it;

public:
    explicit CDBaseIterator(leveldb::Iterator* i, const std::string& first = {}) : it(i)
    {
        assert(it);
        first.empty() ? it->SeekToFirst() : it->Seek(first);
    }

    template<typename T>
    explicit CDBaseIterator(leveldb::Iterator* i, const T& key) : prefix(T::prefix), use_prefix(true), it(i)
    {
        assert(it);
        static_assert(sizeof(T::prefix) == 1, "Prefix needs to be 1 byte");
        CDataStream ssKey(SER_DISK, CLIENT_VERSION, prefix, key);
        it->Seek({ssKey.data(), ssKey.size()});
    }

    CDBaseIterator(CDBaseIterator&&) = default;

    CDBaseIterator& operator++()
    {
        assert(Valid());
        it->Next();
        return *this;
    }

    CDBaseIterator& operator--()
    {
        assert(Valid());
        it->Prev();
        return *this;
    }

    operator bool() const
    {
        return Valid();
    }

    bool Valid() const
    {
        return it->Valid() && (!use_prefix || it->key()[0] == prefix);
    }

    leveldb::Slice Key() const
    {
        assert(Valid());
        return it->key();
    }

    template<typename T>
    T Key() const
    {
        assert(Valid());
        T key;
        try {
            uint8_t prefix;
            auto slKey = it->key();
            CDataStream ssKey(slKey.data(), slKey.data() + slKey.size(), SER_DISK, CLIENT_VERSION);
            ssKey >> prefix >> key;
        } catch (const std::exception&) {
            return {};
        }
        return key;
    }

    leveldb::Slice Value() const
    {
        assert(Valid());
        return it->value();
    }

    template<typename T>
    bool Value(T&& value) const
    {
        assert(Valid());
        try {
            auto slValue = it->value();
            CDataStream ssValue(slValue.data(), slValue.data() + slValue.size(), SER_DISK, CLIENT_VERSION);
            ssValue >> value;
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }
};

#endif // XEP_OMNICORE_DBBASE_H
