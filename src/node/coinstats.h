// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Xep Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XEP_NODE_COINSTATS_H
#define XEP_NODE_COINSTATS_H

#include <amount.h>
#include <uint256.h>

#include <cstdint>

class CCoinsView;

struct CCoinsStats
{
    int nHeight{0};
    uint256 hashBlock{};
    uint64_t nTransactions{0};
    uint64_t nTransactionOutputs{0};
    uint64_t nBogoSize{0};
    uint256 hashSerialized{};
    uint64_t nDiskSize{0};
    CAmount nTotalAmount{0};

    //! The number of coins contained.
    uint64_t coins_count{0};
};

//! Calculate statistics about the unspent transaction output set
bool GetUTXOStats(CCoinsView* view, CCoinsStats& stats);

#endif // XEP_NODE_COINSTATS_H
