// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_H
#define BITCOIN_POW_H

#include <consensus/params.h>

#include <stdint.h>
#include <sync.h>

class CBlockHeader;
class CBlockIndex;
class uint256;

using RecursiveMutex = AnnotatedMixin<std::recursive_mutex>;

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader* pblock, const Consensus::Params&);
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);
unsigned int GetNextWorkRequiredBTC(const CBlockIndex* pindexLast, const CBlockHeader* pblock, const Consensus::Params&);
unsigned int WeightedTargetExponentialMovingAverage(const CBlockIndex* pindexLast, const CBlockHeader* pblock, const Consensus::Params&);
unsigned int AverageTargetASERT(const CBlockIndex* pindexLast, const CBlockHeader* pblock, const Consensus::Params&);
/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(const uint256& hash, const unsigned int& nBits, const int& algo, const Consensus::Params&);

#endif // BITCOIN_POW_H
