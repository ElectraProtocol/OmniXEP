// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Xep Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XEP_CHAIN_H
#define XEP_CHAIN_H

#include <arith_uint256.h>
#include <consensus/params.h>
#include <flatfile.h>
#include <primitives/block.h>
#include <tinyformat.h>
#include <uint256.h>

#include <util/moneystr.h>

#include <vector>

/**
 * Maximum amount of time that a block timestamp is allowed to exceed the
 * current network-adjusted time before the block will be accepted.
 */
static constexpr int64_t MAX_FUTURE_BLOCK_TIME = 15; // FTL = 15 seconds (should be <= consensus.nStakeTimestampMask and <= 1/20 of the difficulty averaging window consensus.nPowTargetTimespan)

/**
 * Timestamp window used as a grace period by code that compares external
 * timestamps (such as timestamps passed to RPCs, or wallet key creation times)
 * to block timestamps. This should be set at least as high as
 * MAX_FUTURE_BLOCK_TIME.
 */
static constexpr int64_t TIMESTAMP_WINDOW = MAX_FUTURE_BLOCK_TIME;

/**
 * Maximum gap between node time and block time used
 * for the "Catching up..." mode in GUI.
 *
 * Ref: https://github.com/xep/xep/pull/1026
 */
static constexpr int64_t MAX_BLOCK_TIME_GAP = 90 * 60;

class CBlockFileInfo
{
public:
    unsigned int nBlocks;      //!< number of blocks stored in file
    unsigned int nSize;        //!< number of used bytes of block file
    unsigned int nUndoSize;    //!< number of used bytes in the undo file
    unsigned int nHeightFirst; //!< lowest height of block in file
    unsigned int nHeightLast;  //!< highest height of block in file
    uint64_t nTimeFirst;       //!< earliest time of block in file
    uint64_t nTimeLast;        //!< latest time of block in file

    SERIALIZE_METHODS(CBlockFileInfo, obj)
    {
        READWRITE(VARINT(obj.nBlocks));
        READWRITE(VARINT(obj.nSize));
        READWRITE(VARINT(obj.nUndoSize));
        READWRITE(VARINT(obj.nHeightFirst));
        READWRITE(VARINT(obj.nHeightLast));
        READWRITE(VARINT(obj.nTimeFirst));
        READWRITE(VARINT(obj.nTimeLast));
    }

     void SetNull() {
         nBlocks = 0;
         nSize = 0;
         nUndoSize = 0;
         nHeightFirst = 0;
         nHeightLast = 0;
         nTimeFirst = 0;
         nTimeLast = 0;
     }

     CBlockFileInfo() {
         SetNull();
     }

     std::string ToString() const;

     /** update statistics (does not update nSize) */
     void AddBlock(unsigned int nHeightIn, uint64_t nTimeIn) {
         if (nBlocks==0 || nHeightFirst > nHeightIn)
             nHeightFirst = nHeightIn;
         if (nBlocks==0 || nTimeFirst > nTimeIn)
             nTimeFirst = nTimeIn;
         nBlocks++;
         if (nHeightIn > nHeightLast)
             nHeightLast = nHeightIn;
         if (nTimeIn > nTimeLast)
             nTimeLast = nTimeIn;
     }
};

enum BlockStatus: uint32_t {
    //! Unused.
    BLOCK_VALID_UNKNOWN      =    0,

    //! Reserved (was BLOCK_VALID_HEADER).
    BLOCK_VALID_RESERVED     =    1,

    //! All parent headers found, difficulty matches, timestamp >= median previous, checkpoint. Implies all parents
    //! are also at least TREE.
    BLOCK_VALID_TREE         =    2,

    /**
     * Only first tx is coinbase, 2 <= coinbase input script length <= 100, transactions valid, no duplicate txids,
     * sigops, size, merkle root. Implies all parents are at least TREE but not necessarily TRANSACTIONS. When all
     * parent blocks also have TRANSACTIONS, CBlockIndex::nChainTx will be set.
     */
    BLOCK_VALID_TRANSACTIONS =    3,

    //! Outputs do not overspend inputs, no double spends, coinbase output ok, no immature coinbase spends, BIP30.
    //! Implies all parents are also at least CHAIN.
    BLOCK_VALID_CHAIN        =    4,

    //! Scripts & signatures ok. Implies all parents are also at least SCRIPTS.
    BLOCK_VALID_SCRIPTS      =    5,

    //! All validity bits.
    BLOCK_VALID_MASK         =   BLOCK_VALID_RESERVED | BLOCK_VALID_TREE | BLOCK_VALID_TRANSACTIONS |
                                 BLOCK_VALID_CHAIN | BLOCK_VALID_SCRIPTS,

    BLOCK_HAVE_DATA          =    8, //!< full block available in blk*.dat
    BLOCK_HAVE_UNDO          =   16, //!< undo data available in rev*.dat
    BLOCK_HAVE_MASK          =   BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO,

    BLOCK_FAILED_VALID       =   32, //!< stage after last reached validness failed
    BLOCK_FAILED_CHILD       =   64, //!< descends from failed block
    BLOCK_FAILED_MASK        =   BLOCK_FAILED_VALID | BLOCK_FAILED_CHILD,

    BLOCK_OPT_WITNESS       =   128, //!< block data in blk*.data was received with a witness-enforcing client
};

/** The block chain is a tree shaped structure starting with the
 * genesis block at the root, with each block potentially having multiple
 * candidates to be the next block. A blockindex may have multiple pprev pointing
 * to it, but at most one of them can be part of the currently active branch.
 */
class CBlockIndex
{
public:
    //! pointer to the hash of the block, if any. Memory is owned by this CBlockIndex
    const uint256* phashBlock{nullptr};

    //! pointer to the index of the predecessor of this block
    CBlockIndex* pprev{nullptr};

    //! pointer to the index of some further predecessor of this block
    CBlockIndex* pskip{nullptr};

    //! height of the entry in the chain. The genesis block has height 0
    int nHeight{0};
    int nHeightPoW{0};
    int nHeightPoS{0};

    //! Which # file this block is stored in (blk?????.dat)
    int nFile{0};

    //! Byte offset within blk?????.dat where this block's data is stored
    unsigned int nDataPos{0};

    //! Byte offset within rev?????.dat where this block's undo data is stored
    unsigned int nUndoPos{0};

    //! (memory only) Total amount of work (expected number of hashes) in the chain up to and including this block
    arith_uint256 nChainWork{};

    //! Number of transactions in this block.
    //! Note: in a potential headers-first mode, this number cannot be relied upon
    unsigned int nTx{0};

    //! (memory only) Number of transactions in the chain up to and including this block.
    //! This value will be non-zero only if and only if transactions for this block and all its parents are available.
    //! Change to 64-bit type when necessary; won't happen before 2030
    unsigned int nChainTx{0};

    //! Verification status of this block. See enum BlockStatus
    uint32_t nStatus{0};

    //! block header
    uint32_t nVersion{0};
    uint256 hashMerkleRoot{};
    uint32_t nTime{0};
    uint32_t nBits{0};
    uint32_t nNonce{0};

    //! (memory only) Sequential id assigned to distinguish order in which blocks are received.
    int32_t nSequenceId{0};

    //! (memory only) Maximum nTime in the chain up to and including this block.
    unsigned int nTimeMax{0};

// peercoin
    // peercoin: money supply related block index fields
    int64_t nMint{0};
    int64_t nMoneySupply{0};
    int64_t nTreasuryPayment{0};

    // peercoin: proof-of-stake related block index fields
    unsigned int nFlags{0}; // peercoin: block index flags
    enum
    {
        BLOCK_PROOF_OF_STAKE = (1 << 0), // is proof-of-stake block
        BLOCK_STAKE_ENTROPY  = (1 << 1), // entropy bit for stake modifier
        BLOCK_STAKE_MODIFIER = (1 << 2), // regenerated stake modifier
        BLOCK_STAKE_MOD_V2   = (1 << 3), // uses nStakeModifierV2
        BLOCK_TREASURY_AWARD = (1 << 4), // is treasury payment block
    };
    uint64_t nStakeModifier{0}; // hash modifier for proof-of-stake
    uint256 nStakeModifierV2{}; // hash modifier for proof-of-stake
    unsigned int nStakeModifierChecksum{0}; // checksum of index; in-memory only
    uint256 hashProofOfStake{};

    bool IsProofOfWork() const
    {
        return !(nFlags & BLOCK_PROOF_OF_STAKE);
    }

    bool IsProofOfStake() const
    {
        return (nFlags & BLOCK_PROOF_OF_STAKE);
    }

    void SetProofOfStake()
    {
        nFlags |= BLOCK_PROOF_OF_STAKE;
    }

    unsigned int GetStakeEntropyBit() const
    {
        return ((nFlags & BLOCK_STAKE_ENTROPY) >> 1);
    }

    bool SetStakeEntropyBit(unsigned int nEntropyBit)
    {
        if (nEntropyBit > 1)
            return false;
        nFlags |= (nEntropyBit ? BLOCK_STAKE_ENTROPY : 0);
        return true;
    }

    bool GeneratedStakeModifier() const
    {
        return (nFlags & BLOCK_STAKE_MODIFIER);
    }

    bool UsesStakeModifierV2() const
    {
        return (nFlags & BLOCK_STAKE_MOD_V2);
    }

    void SetStakeModifier(uint64_t nModifier, bool fGeneratedStakeModifier)
    {
        nStakeModifier = nModifier;
        if (fGeneratedStakeModifier)
            nFlags |= BLOCK_STAKE_MODIFIER;
    }

    void SetStakeModifierV2(uint256 nModifier, bool fGeneratedStakeModifier)
    {
        nStakeModifierV2 = nModifier;
        if (fGeneratedStakeModifier)
            nFlags |= BLOCK_STAKE_MODIFIER | BLOCK_STAKE_MOD_V2;
    }
// peercoin end

    bool IsTreasuryBlock() const
    {
        return (nFlags & BLOCK_TREASURY_AWARD);
    }

    void SetTreasuryBlock()
    {
        nFlags |= BLOCK_TREASURY_AWARD;
    }

    CBlockIndex()
    {
    }

    explicit CBlockIndex(const CBlockHeader& block)
        : nVersion{block.nVersion},
          hashMerkleRoot{block.hashMerkleRoot},
          nTime{block.nTime},
          nBits{block.nBits},
          nNonce{block.nNonce}
    {
    }

    FlatFilePos GetBlockPos() const {
        FlatFilePos ret;
        if (nStatus & BLOCK_HAVE_DATA) {
            ret.nFile = nFile;
            ret.nPos  = nDataPos;
        }
        return ret;
    }

    FlatFilePos GetUndoPos() const {
        FlatFilePos ret;
        if (nStatus & BLOCK_HAVE_UNDO) {
            ret.nFile = nFile;
            ret.nPos  = nUndoPos;
        }
        return ret;
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        if (pprev)
            block.hashPrevBlock = pprev->GetBlockHash();
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        return block;
    }

    uint256 GetBlockHash() const
    {
        return *phashBlock;
    }

    /**
     * Check whether this block's and all previous blocks' transactions have been
     * downloaded (and stored to disk) at some point.
     *
     * Does not imply the transactions are consensus-valid (ConnectTip might fail)
     * Does not imply the transactions are still stored on disk. (IsBlockPruned might return true)
     */
    bool HaveTxsDownloaded() const { return nChainTx != 0; }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    int64_t GetBlockTimeMax() const
    {
        return (int64_t)nTimeMax;
    }

    static constexpr int nMedianTimeSpan = 1;

    int64_t GetMedianTimePast() const
    {
        int64_t pmedian[nMedianTimeSpan];
        int64_t* pbegin = &pmedian[nMedianTimeSpan];
        int64_t* pend = &pmedian[nMedianTimeSpan];

        const CBlockIndex* pindex = this;
        for (int i = 0; i < nMedianTimeSpan && pindex; i++, pindex = pindex->pprev)
            *(--pbegin) = pindex->GetBlockTime();

        std::sort(pbegin, pend);
        return pbegin[(pend - pbegin)/2];
    }

    std::string ToString() const
    {
        return strprintf("CBlockIndex(pprev=%p, nFile=%d, nHeight=%d, nMint=%s, nMoneySupply=%s, nFlags=(%s)(%d)(%s)(%s)(%u), nStakeModifier=%016llx, merkle=%s, hashBlock=%s)",
            pprev, nFile, nHeight,
            FormatMoney(nMint), FormatMoney(nMoneySupply),
            GeneratedStakeModifier() ? "MOD" : "-", GetStakeEntropyBit(), IsProofOfStake() ? "PoS" : "PoW", UsesStakeModifierV2() ? "V2" : "V1", IsTreasuryBlock(),
            nStakeModifier,
            hashMerkleRoot.ToString(),
            GetBlockHash().ToString());
    }

    //! Check whether this block index entry is valid up to the passed validity level.
    bool IsValid(enum BlockStatus nUpTo = BLOCK_VALID_TRANSACTIONS) const
    {
        assert(!(nUpTo & ~BLOCK_VALID_MASK)); // Only validity flags allowed.
        if (nStatus & BLOCK_FAILED_MASK)
            return false;
        return ((nStatus & BLOCK_VALID_MASK) >= nUpTo);
    }

    //! Raise the validity level of this block index entry.
    //! Returns true if the validity was changed.
    bool RaiseValidity(enum BlockStatus nUpTo)
    {
        assert(!(nUpTo & ~BLOCK_VALID_MASK)); // Only validity flags allowed.
        if (nStatus & BLOCK_FAILED_MASK)
            return false;
        if ((nStatus & BLOCK_VALID_MASK) < nUpTo) {
            nStatus = (nStatus & ~BLOCK_VALID_MASK) | nUpTo;
            return true;
        }
        return false;
    }

    //! Build the skiplist pointer for this entry.
    void BuildSkip();

    //! Efficiently find an ancestor of this block.
    CBlockIndex* GetAncestor(int height);
    const CBlockIndex* GetAncestor(int height) const;
};

arith_uint256 GetBlockProof(const CBlockIndex& block);
/** Return the time it would take to redo the work difference between from and to, assuming the current hashrate corresponds to the difficulty at tip, in seconds. */
int64_t GetBlockProofEquivalentTime(const CBlockIndex& to, const CBlockIndex& from, const CBlockIndex& tip, const Consensus::Params&);
/** Find the forking point between two chain tips. */
const CBlockIndex* LastCommonAncestor(const CBlockIndex* pa, const CBlockIndex* pb);


/** Used to marshal pointers into hashes for db storage. */
class CDiskBlockIndex : public CBlockIndex
{
public:
    uint256 hashPrev;

    CDiskBlockIndex() {
        hashPrev = uint256();
    }

    explicit CDiskBlockIndex(const CBlockIndex* pindex) : CBlockIndex(*pindex) {
        hashPrev = (pprev ? pprev->GetBlockHash() : uint256());
    }

    SERIALIZE_METHODS(CDiskBlockIndex, obj)
    {
        int _nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH)) READWRITE(VARINT_MODE(_nVersion, VarIntMode::NONNEGATIVE_SIGNED));

        READWRITE(VARINT_MODE(obj.nHeight, VarIntMode::NONNEGATIVE_SIGNED));
        READWRITE(VARINT_MODE(obj.nHeightPoW, VarIntMode::NONNEGATIVE_SIGNED));
        READWRITE(VARINT_MODE(obj.nHeightPoS, VarIntMode::NONNEGATIVE_SIGNED));
        READWRITE(VARINT(obj.nStatus));
        READWRITE(VARINT(obj.nTx));
        if (obj.nStatus & (BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO)) READWRITE(VARINT_MODE(obj.nFile, VarIntMode::NONNEGATIVE_SIGNED));
        if (obj.nStatus & BLOCK_HAVE_DATA) READWRITE(VARINT(obj.nDataPos));
        if (obj.nStatus & BLOCK_HAVE_UNDO) READWRITE(VARINT(obj.nUndoPos));

        READWRITE(obj.nMint);
        READWRITE(obj.nMoneySupply);
        READWRITE(obj.nFlags);
        if (obj.UsesStakeModifierV2()) {
            READWRITE(obj.nStakeModifierV2);
        } else {
            READWRITE(obj.nStakeModifier);
        }
        if (obj.IsTreasuryBlock()) {
            READWRITE(obj.nTreasuryPayment);
        }
        if (obj.IsProofOfStake()) {
            READWRITE(obj.hashProofOfStake);
        }

        // block header
        READWRITE(obj.nVersion);
        READWRITE(obj.hashPrev);
        READWRITE(obj.hashMerkleRoot);
        READWRITE(obj.nTime);
        READWRITE(obj.nBits);
        READWRITE(obj.nNonce);
    }

    uint256 GetBlockHash() const
    {
        CBlockHeader block;
        block.nVersion        = nVersion;
        block.hashPrevBlock   = hashPrev;
        block.hashMerkleRoot  = hashMerkleRoot;
        block.nTime           = nTime;
        block.nBits           = nBits;
        block.nNonce          = nNonce;
        return block.GetHash();
    }


    std::string ToString() const
    {
        std::string str = "CDiskBlockIndex(";
        str += CBlockIndex::ToString();
        str += strprintf("\n                hashBlock=%s, hashPrev=%s)",
            GetBlockHash().ToString(),
            hashPrev.ToString());
        return str;
    }
};

/** An in-memory indexed chain of blocks. */
class CChain {
private:
    std::vector<CBlockIndex*> vChain;

public:
    /** Returns the index entry for the genesis block of this chain, or nullptr if none. */
    CBlockIndex *Genesis() const {
        return vChain.size() > 0 ? vChain[0] : nullptr;
    }

    /** Returns the index entry for the tip of this chain, or nullptr if none. */
    CBlockIndex *Tip() const {
        return vChain.size() > 0 ? vChain[vChain.size() - 1] : nullptr;
    }

    /** Returns the index entry at a particular height in this chain, or nullptr if no such height exists. */
    CBlockIndex *operator[](int nHeight) const {
        if (nHeight < 0 || nHeight >= (int)vChain.size())
            return nullptr;
        return vChain[nHeight];
    }

    /** Compare two chains efficiently. */
    friend bool operator==(const CChain &a, const CChain &b) {
        return a.vChain.size() == b.vChain.size() &&
               a.vChain[a.vChain.size() - 1] == b.vChain[b.vChain.size() - 1];
    }

    /** Efficiently check whether a block is present in this chain. */
    bool Contains(const CBlockIndex *pindex) const {
        return (*this)[pindex->nHeight] == pindex;
    }

    /** Find the successor of a block in this chain, or nullptr if the given index is not found or is the tip. */
    CBlockIndex *Next(const CBlockIndex *pindex) const {
        if (Contains(pindex))
            return (*this)[pindex->nHeight + 1];
        else
            return nullptr;
    }

    /** Return the maximal height in the chain. Is equal to chain.Tip() ? chain.Tip()->nHeight : -1. */
    int Height() const {
        return vChain.size() - 1;
    }

    /** Set/initialize a chain with a given tip. */
    void SetTip(CBlockIndex *pindex);

    /** Return a CBlockLocator that refers to a block in this chain (by default the tip). */
    CBlockLocator GetLocator(const CBlockIndex *pindex = nullptr) const;

    /** Find the last common block between this chain and a block index entry. */
    const CBlockIndex *FindFork(const CBlockIndex *pindex) const;

    /** Find the earliest block with timestamp equal or greater than the given time and height equal or greater than the given height. */
    CBlockIndex* FindEarliestAtLeast(int64_t nTime, int height) const;
};

#endif // XEP_CHAIN_H
