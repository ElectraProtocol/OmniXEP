#include <omnicore/walletutils.h>

#include <omnicore/log.h>
#include <omnicore/omnicore.h>
#include <omnicore/rules.h>
#include <omnicore/script.h>
#include <omnicore/utilsxep.h>

#include <amount.h>
#include <base58.h>
#include <init.h>
#include <interfaces/wallet.h>
#include <key_io.h>
#include <validation.h>
#include <policy/policy.h>
#include <pubkey.h>
#include <script/standard.h>
#include <sync.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/strencodings.h>
#ifdef ENABLE_WALLET
#include <wallet/coincontrol.h>
#include <wallet/ismine.h>
#include <wallet/wallet.h>
#endif

#include <stdint.h>
#include <algorithm>
#include <map>
#include <string>

namespace mastercore
{
/**
 * Retrieves a public key from the wallet, or converts a hex-string to a public key.
 */
bool AddressToPubKey(interfaces::Wallet* iWallet, const std::string& key, CPubKey& pubKey)
{
#ifdef ENABLE_WALLET
    // Case 1: Xep address and the key is in the wallet
    CTxDestination dest = DecodeDestination(key);
    if (IsValidDestination(dest)) {
        CKeyID keyID = iWallet->getKeyForDestination(dest);
        if (keyID.IsNull()) {
            PrintToLog("%s: ERROR: redemption address %s does not refer to a public key\n", __func__, key);
            return false;
        }
        CScript script = GetScriptForDestination(dest);
        if (!iWallet->getPubKey(script, keyID, pubKey)) {
            PrintToLog("%s: ERROR: no public key in wallet for redemption address %s\n", __func__, key);
            return false;
        }
    }
    // Case 2: Hex-encoded public key
    else
#endif
    if (IsHex(key)) {
        pubKey = CPubKey(ParseHex(key));
    }

    if (!pubKey.IsFullyValid()) {
        PrintToLog("%s: ERROR: invalid redemption key %s\n", __func__, key);
        return false;
    }

    return true;
}

/**
 * Checks, whether enough spendable outputs are available to pay for transaction fees.
 */
bool CheckFee(interfaces::Wallet& iWallet, const std::string& fromAddress, size_t nDataSize)
{
    int64_t minFee = 0;
    int64_t feePerKB = 0;
    int64_t inputTotal = 0;
#ifdef ENABLE_WALLET
    bool fUseClassC = UseEncodingClassC(nDataSize);

    CCoinControl coinControl;
    inputTotal = SelectCoins(iWallet, fromAddress, coinControl, 0);

    // calculate the estimated fee per KB based on the currently set confirm target
    FeeCalculation feeCalc;
    CFeeRate feeRate = ::feeEstimator.estimateSmartFee(iWallet.getConfirmTarget(), &feeCalc, true /* FeeEstimateMode::CONSERVATIVE */);

    // if there is not enough data (and zero is estimated) then base minimum on a fairly high/safe 50,000 satoshi fee per KB
    if (feeRate == CFeeRate(0)) {
        feePerKB = 50000;
    } else {
        feePerKB = feeRate.GetFeePerK();
    }

    // we do not know the size of the transaction at this point.  Warning calculation employs some guesswork
    if (!fUseClassC) {
        // Calculation based on a 3KB transaction due to:
        //   - the average size across all Class B transactions ever sent to date is 909 bytes.
        //   - under 2% of Class B transactions are over 2KB, under 0.6% of transactions are over 3KB.
        // Thus if created transaction will be over 3KB (rare as per above) warning may not be sufficient.
        minFee = feePerKB * 3;
    } else {
        // Averages for Class C transactions are not yet available, Calculation based on a 2KB transaction due to:
        //   - Class B values but considering Class C removes outputs for both data and Exodus (reduces size).
        minFee = feePerKB * 2;
    }
#endif
    return inputTotal >= minFee;
}

/**
 * Checks, whether the output qualifies as input for a transaction.
 */
bool CheckInput(const CTxOut& txOut, int nHeight, CTxDestination& dest)
{
    txnouttype whichType;

    if (!GetOutputType(txOut.scriptPubKey, whichType)) {
        return false;
    }
    if (!IsAllowedInputType(whichType, nHeight)) {
        return false;
    }
    if (!ExtractDestination(txOut.scriptPubKey, dest)) {
        return false;
    }

    return true;
}

/**
 * IsMine wrapper to determine whether the address is in the wallet.
 */
int IsMyAddress(const std::string& address, interfaces::Wallet* iWallet)
{
#ifdef ENABLE_WALLET
    if (iWallet) {
        CTxDestination destination = DecodeDestination(address);
        return static_cast<int>(iWallet->isMine(destination));
    }
#endif
    return 0;
}

/**
 * IsMine wrapper to determine whether the address is in the wallet.
 */
int IsMyAddressAllWallets(const std::string& address, const bool matchAny, const isminefilter& filter)
{
#ifdef ENABLE_WALLET
    if (!HasWallets())
        return 0;

    CTxDestination destination = DecodeDestination(address);
    for(const std::shared_ptr<CWallet> wallet : GetWallets()) {
        isminetype ismine = WITH_LOCK(wallet->cs_wallet, return wallet->IsMine(destination));
        if (matchAny && ismine != ISMINE_NO)
            return static_cast<int>(ismine);
        else if (ismine & filter)
            return static_cast<int>(ismine);
    }
#endif
    return 0;
}

/**
 * Estimate the minimum fee considering user set parameters and the required fee.
 *
 * @return The estimated fee per 1000 byte, or 50000 satoshi as per default
 */
CAmount GetEstimatedFeePerKb(interfaces::Wallet& iWallet)
{
    // Need to look over this to see if we need to increase for XEP
    CAmount nFee = 50000; // 0.0005 XEP;

#ifdef ENABLE_WALLET
    CCoinControl coinControl;
    nFee = iWallet.getMinimumFee(100000, coinControl, nullptr, nullptr);
#endif

    return nFee;
}

/**
 * Output values below this value are considered uneconomic, because it would
 * require more fees to pay than the output is worth.
 *
 * @param txOut[in]  The transaction output to check
 * @return The minimum value an output to spent should have
 */
int64_t GetEconomicThreshold(interfaces::Wallet& iWallet, const CTxOut& txOut)
{
    // Minimum value needed to relay the transaction
    int64_t nThresholdDust = GetDustThreshold(txOut, minRelayTxFee);

    // Use the estimated fee that is also used to construct transactions.
    // We use the absolute minimum, so we divide by 3, to get rid of the
    // safety margin used for the dust threshold used for relaying.
    CFeeRate estimatedFeeRate(GetEstimatedFeePerKb(iWallet));
    int64_t nThresholdFees = GetDustThreshold(txOut, estimatedFeeRate) / 3;

    return std::max(nThresholdDust, nThresholdFees);
}

#ifdef ENABLE_WALLET
int64_t SelectCoins(interfaces::Wallet& iWallet, const std::string& fromAddress, CCoinControl& coinControl, int64_t amountRequired)
{
    // total output funds collected
    int64_t nTotal = 0;
    int nHeight = mastercore::GetHeight();

    std::map<uint256, interfaces::WalletTxStatus> tx_status;
    const std::vector<interfaces::WalletTx>& transactions = iWallet.getWalletTxsDetails(tx_status);

    // iterate over the wallet
    for (std::vector<interfaces::WalletTx>::const_iterator it = transactions.begin(); it != transactions.end(); ++it) {
        const CTransactionRef tx = it->tx;
        const uint256& txid = tx->GetHash();

        auto status = tx_status.find(txid);
        if (status == tx_status.end() || !status->second.is_trusted) {
            continue;
        }

        if (status->second.depth_in_main_chain == 0) {
            LOCK(mempool.cs);
            if (!mempool.exists(txid)) {
                continue;
            }
        }

        if (!it->available_credit) {
            continue;
        }

        for (unsigned int n = 0; n < tx->vout.size(); n++) {
            const CTxOut& txOut = tx->vout[n];

            CTxDestination dest;
            if (!CheckInput(txOut, nHeight, dest)) {
                continue;
            }
            if (!iWallet.txoutIsMine(txOut)) {
                continue;
            }
            if (iWallet.isSpent(txid, n)) {
                continue;
            }
            if (iWallet.isLockedCoin(COutPoint(txid, n))) {
                continue;
            }
            if (txOut.nValue < GetEconomicThreshold(iWallet, txOut)) {
                if (msc_debug_tokens)
                    PrintToLog("%s: output value below economic threshold: %s:%d, value: %d\n",
                            __func__, txid.GetHex(), n, txOut.nValue);
                continue;
            }

            std::string sAddress = EncodeDestination(dest);

            // only use funds from the sender's address
            if (fromAddress == sAddress) {
                COutPoint outpoint(txid, n);
                coinControl.Select(outpoint);

                nTotal += txOut.nValue;

                if (msc_debug_tokens) {
                    PrintToLog("%s: selecting sender: %s, outpoint: %s:%d, value: %d\n", __func__, sAddress, txid.GetHex(), n, txOut.nValue);
                }

                if (amountRequired <= nTotal) {
                    break;
                }
            }
        }

        if (amountRequired <= nTotal) {
            break;
        }
    }

    return nTotal;
}

int64_t SelectAllCoins(interfaces::Wallet& iWallet, const std::string& fromAddress, CCoinControl& coinControl)
{
    // total output funds collected
    int64_t nTotal = 0;
    int nHeight = mastercore::GetHeight();

    std::map<uint256, interfaces::WalletTxStatus> tx_status;
    const std::vector<interfaces::WalletTx>& transactions = iWallet.getWalletTxsDetails(tx_status);

    // iterate over the wallet
    for (std::vector<interfaces::WalletTx>::const_iterator it = transactions.begin(); it != transactions.end(); ++it) {
        const CTransactionRef tx = it->tx;
        const uint256& txid = tx->GetHash();

        auto status = tx_status.find(txid);
        if (status == tx_status.end() || !status->second.is_trusted) {
            continue;
        }

        if (!it->available_credit) {
            continue;
        }

        for (unsigned int n = 0; n < tx->vout.size(); n++) {
            const CTxOut& txOut = tx->vout[n];

            CTxDestination dest;
            if (!CheckInput(txOut, nHeight, dest)) {
                continue;
            }
            if (!iWallet.isSpendable(dest)) {
                continue;
            }
            if (iWallet.isSpent(txid, n)) {
                continue;
            }
            if (iWallet.isLockedCoin(COutPoint(txid, n))) {
                continue;
            }

            std::string sAddress = EncodeDestination(dest);
            if (msc_debug_tokens) {
                PrintToLog("%s: sender: %s, outpoint: %s:%d, value: %d\n", __func__, sAddress, txid.GetHex(), n, txOut.nValue);
            }

            // only use funds from the sender's address
            if (fromAddress == sAddress) {
                COutPoint outpoint(txid, n);
                coinControl.Select(outpoint);

                nTotal += txOut.nValue;
            }
        }
    }

    return nTotal;
}
#endif

} // namespace mastercore
