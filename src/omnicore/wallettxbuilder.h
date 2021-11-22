#ifndef XEP_OMNICORE_WALLETTXBUILDER_H
#define XEP_OMNICORE_WALLETTXBUILDER_H

#if defined(HAVE_CONFIG_H)
#include <config/xep-config.h>
#endif

class uint256;
class CWallet;
struct NodeContext;

namespace interfaces {
class Wallet;
} // namespace interfaces

#include <amount.h>

#include <stdint.h>
#include <string>
#include <tuple>
#include <vector>

/**
 * Creates and sends a transaction.
 */
int WalletTxBuilder(
        const std::string& senderAddress,
        const std::string& receiverAddress,
        const std::string& redemptionAddress,
        int64_t referenceAmount,
        const std::vector<unsigned char>& payload,
        uint256& retTxid,
        std::string& retRawTx,
        bool commit,
        interfaces::Wallet* iWallet = nullptr,
        CAmount minFee = 0);

/**
 * Creates and sends a transaction with multiple receivers.
 */
int WalletTxBuilder(
        const std::string& senderAddress,
        const std::vector<std::string>& receiverAddresses,
        const std::string& redemptionAddress,
        int64_t referenceAmount,
        const std::vector<unsigned char>& payload,
        uint256& retTxid,
        std::string& retRawTx,
        bool commit,
        interfaces::Wallet* iWallet = nullptr,
        CAmount minFee = 0);

/**
 * Simulates the creation of a payload to count the required outputs.
 */
int GetDryPayloadOutputCount(
        const std::string& senderAddress,
        const std::string& redemptionAddress,
        const std::vector<unsigned char>& payload,
        interfaces::Wallet* iWallet);

#ifdef ENABLE_WALLET
/**
 * Creates and sends a raw transaction by selecting all coins from the sender
 * and enough coins from a fee source. Change is sent to the fee source!
 */
int CreateFundedTransaction(const std::string& senderAddress,
        const std::string& receiverAddress,
        const std::string& feeAddress,
        const std::vector<unsigned char>& payload,
        uint256& retTxid,
        interfaces::Wallet* iWallet,
        NodeContext &node);

int CreateDExTransaction(interfaces::Wallet* pwallet, const std::string& buyerAddress, const std::string& sellerAddress, const CAmount& nAmount, uint256& txid);
#endif

#endif // XEP_OMNICORE_WALLETTXBUILDER_H
