/**
 * @file rules.cpp
 *
 * This file contains consensus rules and restrictions.
 */

#include <omnicore/rules.h>

#include <omnicore/activation.h>
#include <omnicore/consensushash.h>
#include <omnicore/dbtxlist.h>
#include <omnicore/log.h>
#include <omnicore/omnicore.h>
#include <omnicore/notifications.h>
#include <omnicore/utilsxep.h>
#include <omnicore/version.h>

#include <chainparams.h>
#include <validation.h>
#include <script/standard.h>
#include <uint256.h>
#include <ui_interface.h>

#include <stdint.h>
#include <limits>
#include <string>
#include <vector>

namespace mastercore
{
/**
 * Returns a mapping of transaction types, and the blocks at which they are enabled.
 */
std::vector<TransactionRestriction> CConsensusParams::GetRestrictions() const
{
    const TransactionRestriction vTxRestrictions[] =
    { //  transaction type                    version        allow 0  activation block
      //  ----------------------------------  -------------  -------  ------------------
        { OMNICORE_MESSAGE_TYPE_ALERT,        0xFFFF,        true,    MSC_ALERT_BLOCK           },
        { OMNICORE_MESSAGE_TYPE_ACTIVATION,   0xFFFF,        true,    MSC_ALERT_BLOCK           },
        { OMNICORE_MESSAGE_TYPE_DEACTIVATION, 0xFFFF,        true,    MSC_ALERT_BLOCK           },

        { MSC_TYPE_SIMPLE_SEND,               MP_TX_PKT_V0,  false,   MSC_SEND_BLOCK            },

        { MSC_TYPE_TRADE_OFFER,               MP_TX_PKT_V0,  false,   MSC_DEX_BLOCK             },
        { MSC_TYPE_TRADE_OFFER,               MP_TX_PKT_V1,  false,   MSC_DEX_BLOCK             },
        { MSC_TYPE_ACCEPT_OFFER_XEP,          MP_TX_PKT_V0,  false,   MSC_DEX_BLOCK             },

        { MSC_TYPE_CREATE_PROPERTY_FIXED,     MP_TX_PKT_V0,  false,   MSC_SP_BLOCK              },
        { MSC_TYPE_CREATE_PROPERTY_VARIABLE,  MP_TX_PKT_V0,  false,   MSC_SP_BLOCK              },
        { MSC_TYPE_CREATE_PROPERTY_VARIABLE,  MP_TX_PKT_V1,  false,   MSC_SP_BLOCK              },
        { MSC_TYPE_CREATE_PROPERTY_VARIABLE,  MP_TX_PKT_V2,  false,   MSC_XEP_CROWDSALE_BLOCK   },
        { MSC_TYPE_CLOSE_CROWDSALE,           MP_TX_PKT_V0,  false,   MSC_SP_BLOCK              },

        { MSC_TYPE_XEP_PAYMENT,               MP_TX_PKT_V0,   true,   MSC_XEP_CROWDSALE_BLOCK   },

        { MSC_TYPE_CREATE_PROPERTY_MANUAL,    MP_TX_PKT_V0,  false,   MSC_MANUALSP_BLOCK        },
        { MSC_TYPE_GRANT_PROPERTY_TOKENS,     MP_TX_PKT_V0,  false,   MSC_MANUALSP_BLOCK        },
        { MSC_TYPE_REVOKE_PROPERTY_TOKENS,    MP_TX_PKT_V0,  false,   MSC_MANUALSP_BLOCK        },
        { MSC_TYPE_CHANGE_ISSUER_ADDRESS,     MP_TX_PKT_V0,  false,   MSC_MANUALSP_BLOCK        },
        { MSC_TYPE_ENABLE_FREEZING,           MP_TX_PKT_V0,  false,   MSC_MANUALSP_BLOCK        },
        { MSC_TYPE_DISABLE_FREEZING,          MP_TX_PKT_V0,  false,   MSC_MANUALSP_BLOCK        },
        { MSC_TYPE_FREEZE_PROPERTY_TOKENS,    MP_TX_PKT_V0,  false,   MSC_MANUALSP_BLOCK        },
        { MSC_TYPE_UNFREEZE_PROPERTY_TOKENS,  MP_TX_PKT_V0,  false,   MSC_MANUALSP_BLOCK        },

        { MSC_TYPE_ADD_DELEGATE,              MP_TX_PKT_V0,  false,   MSC_DELEGATED_ISSUANCE_BLOCK },
        { MSC_TYPE_REMOVE_DELEGATE,           MP_TX_PKT_V0,  false,   MSC_DELEGATED_ISSUANCE_BLOCK },

        { MSC_TYPE_SEND_TO_OWNERS,            MP_TX_PKT_V0,  false,   MSC_STO_BLOCK      },
        { MSC_TYPE_SEND_TO_OWNERS,            MP_TX_PKT_V1,  false,   MSC_STOV1_BLOCK    },

        { MSC_TYPE_METADEX_TRADE,             MP_TX_PKT_V0,  false,   MSC_METADEX_BLOCK  },
        { MSC_TYPE_METADEX_CANCEL_PRICE,      MP_TX_PKT_V0,  false,   MSC_METADEX_BLOCK  },
        { MSC_TYPE_METADEX_CANCEL_PAIR,       MP_TX_PKT_V0,  false,   MSC_METADEX_BLOCK  },
        { MSC_TYPE_METADEX_CANCEL_ECOSYSTEM,  MP_TX_PKT_V0,  false,   MSC_METADEX_BLOCK  },

        { MSC_TYPE_SEND_ALL,                  MP_TX_PKT_V0,  false,   MSC_SEND_ALL_BLOCK },
        
        { MSC_TYPE_ANYDATA,                   MP_TX_PKT_V0,  true,    MSC_ANYDATA_BLOCK },

        { MSC_TYPE_OFFER_ACCEPT_A_BET,        MP_TX_PKT_V0,  false,   MSC_BET_BLOCK      },

        { MSC_TYPE_SEND_NONFUNGIBLE,          MP_TX_PKT_V0,  false,   MSC_NONFUNGIBLE_BLOCK  },
        { MSC_TYPE_NONFUNGIBLE_DATA,          MP_TX_PKT_V0,  false,   MSC_NONFUNGIBLE_BLOCK  },

        { MSC_TYPE_SEND_TO_MANY,              MP_TX_PKT_V0,  false,   MSC_SEND_TO_MANY_BLOCK  },
    };

    const size_t nSize = sizeof(vTxRestrictions) / sizeof(vTxRestrictions[0]);

    return std::vector<TransactionRestriction>(vTxRestrictions, vTxRestrictions + nSize);
}

/**
 * Returns an empty vector of consensus checkpoints.
 *
 * This method should be overwritten by the child classes, if needed.
 */
std::vector<ConsensusCheckpoint> CConsensusParams::GetCheckpoints() const
{
    return std::vector<ConsensusCheckpoint>();
}

/**
 * Returns an empty vector of transaction checkpoints.
 *
 * This method should be overwritten by the child classes, if needed.
 */
std::vector<TransactionCheckpoint> CConsensusParams::GetTransactions() const
{
    return std::vector<TransactionCheckpoint>();
}

/**
 * Returns consensus checkpoints for mainnet, used to verify transaction processing.
 */
std::vector<ConsensusCheckpoint> CMainConsensusParams::GetCheckpoints() const
{
    // block height, block hash and consensus hash
    const ConsensusCheckpoint vCheckpoints[] = {

    };

    const size_t nSize = sizeof(vCheckpoints) / sizeof(vCheckpoints[0]);

    return std::vector<ConsensusCheckpoint>(vCheckpoints, vCheckpoints + nSize);
}

/**
 * Returns transactions checkpoints for mainnet, used to verify DB consistency.
 */
std::vector<TransactionCheckpoint> CMainConsensusParams::GetTransactions() const
{
    // block height, transaction hash
    const TransactionCheckpoint vTransactions[] = {

    };

    const size_t nSize = sizeof(vTransactions) / sizeof(vTransactions[0]);

    return std::vector<TransactionCheckpoint>(vTransactions, vTransactions + nSize);
}

/**
 * Constructor for mainnet consensus parameters.
 */
CMainConsensusParams::CMainConsensusParams()
{
    // Exodus related:
    exodusBonusPerWeek = 0;   //No bonus
    exodusDeadline = 1698809526; // Wed Nov 01 2023 03:32:06 GMT+0000
    exodusReward = 1;         // 1:1 ratio
    GENESIS_BLOCK = 1317100;
    LAST_EXODUS_BLOCK = std::numeric_limits<int>::max(); // No end so far
    // Notice range for feature activations:
    MIN_ACTIVATION_BLOCKS = 2048;  // ~2 weeks
    MAX_ACTIVATION_BLOCKS = 12288; // ~12 weeks
    // Waiting period for enabling freezing
    OMNI_FREEZE_WAIT_PERIOD = 4096; // ~4 weeks
    // Script related:
    PUBKEYHASH_BLOCK = GENESIS_BLOCK;
    SCRIPTHASH_BLOCK = GENESIS_BLOCK;
    MULTISIG_BLOCK = GENESIS_BLOCK;
    NULLDATA_BLOCK = GENESIS_BLOCK;
    // Transaction restrictions:
    MSC_ALERT_BLOCK = GENESIS_BLOCK;
    MSC_SEND_BLOCK = GENESIS_BLOCK;
    MSC_DEX_BLOCK = GENESIS_BLOCK;
    MSC_SP_BLOCK = GENESIS_BLOCK;
    MSC_MANUALSP_BLOCK = GENESIS_BLOCK;
    MSC_STO_BLOCK = GENESIS_BLOCK;
    MSC_METADEX_BLOCK = GENESIS_BLOCK;
    MSC_SEND_ALL_BLOCK = GENESIS_BLOCK;
    MSC_BET_BLOCK = std::numeric_limits<int>::max();
    MSC_STOV1_BLOCK = std::numeric_limits<int>::max(); //TODO next update: activate at current height
    MSC_XEP_CROWDSALE_BLOCK = GENESIS_BLOCK;
    MSC_ANYDATA_BLOCK = GENESIS_BLOCK;
    MSC_NONFUNGIBLE_BLOCK = GENESIS_BLOCK;
    MSC_DELEGATED_ISSUANCE_BLOCK = GENESIS_BLOCK;
    MSC_SEND_TO_MANY_BLOCK = std::numeric_limits<int>::max();
    // Other feature activations:
    GRANTEFFECTS_FEATURE_BLOCK = GENESIS_BLOCK;
    DEXMATH_FEATURE_BLOCK = GENESIS_BLOCK;
    SPCROWDCROSSOVER_FEATURE_BLOCK = GENESIS_BLOCK;
    TRADEALLPAIRS_FEATURE_BLOCK = GENESIS_BLOCK;
    FEES_FEATURE_BLOCK = std::numeric_limits<int>::max();
    FREEZENOTICE_FEATURE_BLOCK = std::numeric_limits<int>::max();
    FREEDEX_FEATURE_BLOCK = GENESIS_BLOCK;
    NONFUNGIBLETOKEN_ISSUER_DATA = std::numeric_limits<int>::max();
}

/**
 * Constructor for testnet consensus parameters.
 */
CTestNetConsensusParams::CTestNetConsensusParams()
{
    // Exodus related:
    exodusBonusPerWeek = 0.00;
    exodusDeadline = 1377993600;
    exodusReward = 0;
    GENESIS_BLOCK = 0;
    LAST_EXODUS_BLOCK = std::numeric_limits<int>::max();
    // Notice range for feature activations:
    MIN_ACTIVATION_BLOCKS = 0;
    MAX_ACTIVATION_BLOCKS = std::numeric_limits<int>::max();
    // Waiting period for enabling freezing
    OMNI_FREEZE_WAIT_PERIOD = 0;
    // Script related:
    PUBKEYHASH_BLOCK = 0;
    SCRIPTHASH_BLOCK = 0;
    MULTISIG_BLOCK = 0;
    NULLDATA_BLOCK = 0;
    // Transaction restrictions:
    MSC_ALERT_BLOCK = 0;
    MSC_SEND_BLOCK = 0;
    MSC_DEX_BLOCK = 0;
    MSC_SP_BLOCK = 0;
    MSC_MANUALSP_BLOCK = 0;
    MSC_STO_BLOCK = 0;
    MSC_METADEX_BLOCK = 0;
    MSC_SEND_ALL_BLOCK = 0;
    MSC_BET_BLOCK = 999999;
    MSC_STOV1_BLOCK = 0;
    MSC_XEP_CROWDSALE_BLOCK = 0;
    MSC_ANYDATA_BLOCK = 0;
    MSC_NONFUNGIBLE_BLOCK = 0;
    MSC_DELEGATED_ISSUANCE_BLOCK = 0;
    MSC_SEND_TO_MANY_BLOCK = 0;
    // Other feature activations:
    GRANTEFFECTS_FEATURE_BLOCK = 0;
    DEXMATH_FEATURE_BLOCK = 0;
    SPCROWDCROSSOVER_FEATURE_BLOCK = 0;
    TRADEALLPAIRS_FEATURE_BLOCK = 0;
    FEES_FEATURE_BLOCK = 0;
    FREEZENOTICE_FEATURE_BLOCK = 0;
    FREEDEX_FEATURE_BLOCK = 0;
    NONFUNGIBLETOKEN_ISSUER_DATA = 0;
}

/**
 * Constructor for regtest consensus parameters.
 */
CRegTestConsensusParams::CRegTestConsensusParams()
{
    // Exodus related:
    exodusBonusPerWeek = 0.00;
    exodusDeadline = 1377993600;
    exodusReward = 0;
    GENESIS_BLOCK = std::numeric_limits<int>::max();
    LAST_EXODUS_BLOCK = std::numeric_limits<int>::max();
    // Notice range for feature activations:
    MIN_ACTIVATION_BLOCKS = 5;
    MAX_ACTIVATION_BLOCKS = 10;
    // Waiting period for enabling freezing
    OMNI_FREEZE_WAIT_PERIOD = 10;
    // Script related:
    PUBKEYHASH_BLOCK = 0;
    SCRIPTHASH_BLOCK = 0;
    MULTISIG_BLOCK = 0;
    NULLDATA_BLOCK = 0;
    // Transaction restrictions:
    MSC_ALERT_BLOCK = 0;
    MSC_SEND_BLOCK = 0;
    MSC_DEX_BLOCK = 0;
    MSC_SP_BLOCK = 0;
    MSC_MANUALSP_BLOCK = 0;
    MSC_STO_BLOCK = 0;
    MSC_METADEX_BLOCK = 0;
    MSC_SEND_ALL_BLOCK = 0;
    MSC_BET_BLOCK = 999999;
    MSC_STOV1_BLOCK = 999999;
    MSC_XEP_CROWDSALE_BLOCK = 999999;
    MSC_ANYDATA_BLOCK = 0;
    MSC_NONFUNGIBLE_BLOCK = 0;
    MSC_DELEGATED_ISSUANCE_BLOCK = 0;
    MSC_SEND_TO_MANY_BLOCK = 0;
    // Other feature activations:
    GRANTEFFECTS_FEATURE_BLOCK = 999999;
    DEXMATH_FEATURE_BLOCK = 999999;
    SPCROWDCROSSOVER_FEATURE_BLOCK = 999999;
    TRADEALLPAIRS_FEATURE_BLOCK = 999999;
    FEES_FEATURE_BLOCK = 999999;
    FREEZENOTICE_FEATURE_BLOCK = 999999;
    FREEDEX_FEATURE_BLOCK = 999999;
    NONFUNGIBLETOKEN_ISSUER_DATA = 999999;
}

//! Consensus parameters for mainnet
static CMainConsensusParams mainConsensusParams;
//! Consensus parameters for testnet
static CTestNetConsensusParams testNetConsensusParams;
//! Consensus parameters for regtest mode
static CRegTestConsensusParams regTestConsensusParams;

/**
 * Returns consensus parameters for the given network.
 */
CConsensusParams& ConsensusParams(const std::string& network)
{
    if (network == "main") {
        return mainConsensusParams;
    }
    if (network == "test") {
        return testNetConsensusParams;
    }
    if (network == "regtest") {
        return regTestConsensusParams;
    }
    // Fallback:
    return mainConsensusParams;
}

/**
 * Returns currently active consensus parameter.
 */
const CConsensusParams& ConsensusParams()
{
    const std::string& network = Params().NetworkIDString();

    return ConsensusParams(network);
}

/**
 * Returns currently active mutable consensus parameter.
 */
CConsensusParams& MutableConsensusParams()
{
    const std::string& network = Params().NetworkIDString();

    return ConsensusParams(network);
}

/**
 * Resets consensus parameters.
 */
void ResetConsensusParams()
{
    mainConsensusParams = CMainConsensusParams();
    testNetConsensusParams = CTestNetConsensusParams();
    regTestConsensusParams = CRegTestConsensusParams();
}

/**
 * Checks, if the script type is allowed as input.
 */
bool IsAllowedInputType(int whichType, int nBlock)
{
    const CConsensusParams& params = ConsensusParams();

    switch (whichType)
    {
        case TX_PUBKEYHASH:
            return (params.PUBKEYHASH_BLOCK <= nBlock);

        case TX_SCRIPTHASH:
            return (params.SCRIPTHASH_BLOCK <= nBlock);
    }

    return false;
}

/**
 * Checks, if the script type qualifies as output.
 */
bool IsAllowedOutputType(int whichType, int nBlock)
{
    const CConsensusParams& params = ConsensusParams();

    switch (whichType)
    {
        case TX_PUBKEYHASH:
            return (params.PUBKEYHASH_BLOCK <= nBlock);

        case TX_SCRIPTHASH:
            return (params.SCRIPTHASH_BLOCK <= nBlock);

        case TX_MULTISIG:
            return (params.MULTISIG_BLOCK <= nBlock);

        case TX_NULL_DATA:
            return (params.NULLDATA_BLOCK <= nBlock);
    }

    return false;
}

/**
 * Whether Xep payments are supported for a transaction type/version.
 */
bool IsXepPaymentAllowed(uint16_t type, uint16_t version)
{
    bool allowed = false;

    if (type == MSC_TYPE_CREATE_PROPERTY_VARIABLE && version == MP_TX_PKT_V2) {
            allowed = true;
    }

    return allowed;
}

/**
 * Activates a feature at a specific block height, authorization has already been validated.
 *
 * Note: Feature activations are consensus breaking.  It is not permitted to activate a feature within
 *       the next 2048 blocks (roughly 2 weeks), nor is it permitted to activate a feature further out
 *       than 12288 blocks (roughly 12 weeks) to ensure sufficient notice.
 *       This does not apply for activation during initialization (where loadingActivations is set true).
 */
bool ActivateFeature(uint16_t featureId, int activationBlock, uint32_t minClientVersion, int transactionBlock)
{
    PrintToLog("Feature activation requested (ID %d to go active as of block: %d)\n", featureId, activationBlock);

    const CConsensusParams& params = ConsensusParams();

    // check activation block is allowed
    if ((activationBlock < (transactionBlock + params.MIN_ACTIVATION_BLOCKS)) ||
        (activationBlock > (transactionBlock + params.MAX_ACTIVATION_BLOCKS))) {
            PrintToLog("Feature activation of ID %d refused due to notice checks\n", featureId);
            return false;
    }

    // check whether the feature is already active
    if (IsFeatureActivated(featureId, transactionBlock)) {
        PrintToLog("Feature activation of ID %d refused as the feature is already live\n", featureId);
        return false;
    }

    // check feature is recognized and activation is successful
    std::string featureName = GetFeatureName(featureId);
    bool supported = OMNICORE_VERSION >= minClientVersion;
    switch (featureId) {
        case FEATURE_CLASS_C:
            MutableConsensusParams().NULLDATA_BLOCK = activationBlock;
        break;
        case FEATURE_METADEX:
            MutableConsensusParams().MSC_METADEX_BLOCK = activationBlock;
        break;
        case FEATURE_BETTING:
            MutableConsensusParams().MSC_BET_BLOCK = activationBlock;
        break;
        case FEATURE_GRANTEFFECTS:
            MutableConsensusParams().GRANTEFFECTS_FEATURE_BLOCK = activationBlock;
        break;
        case FEATURE_DEXMATH:
            MutableConsensusParams().DEXMATH_FEATURE_BLOCK = activationBlock;
        break;
        case FEATURE_SENDALL:
            MutableConsensusParams().MSC_SEND_ALL_BLOCK = activationBlock;
        break;
        case FEATURE_SPCROWDCROSSOVER:
            MutableConsensusParams().SPCROWDCROSSOVER_FEATURE_BLOCK = activationBlock;
        break;
        case FEATURE_TRADEALLPAIRS:
            MutableConsensusParams().TRADEALLPAIRS_FEATURE_BLOCK = activationBlock;
        break;
        case FEATURE_FEES:
            MutableConsensusParams().FEES_FEATURE_BLOCK = activationBlock;
        break;
        case FEATURE_STOV1:
            MutableConsensusParams().MSC_STOV1_BLOCK = activationBlock;
        break;
        case FEATURE_FREEZENOTICE:
            MutableConsensusParams().FREEZENOTICE_FEATURE_BLOCK = activationBlock;
        break;
        case FEATURE_FREEDEX:
            MutableConsensusParams().FREEDEX_FEATURE_BLOCK = activationBlock;
        break;
        case FEATURE_NONFUNGIBLE:
            MutableConsensusParams().MSC_NONFUNGIBLE_BLOCK = activationBlock;
        break;
        case FEATURE_NONFUNGIBLE_ISSUER:
            MutableConsensusParams().NONFUNGIBLETOKEN_ISSUER_DATA = activationBlock;
        break;
        case FEATURE_DELEGATEDISSUANCE:
            MutableConsensusParams().MSC_DELEGATED_ISSUANCE_BLOCK = activationBlock;
        break;
        case FEATURE_SEND_TO_MANY:
            MutableConsensusParams().MSC_SEND_TO_MANY_BLOCK = activationBlock;
        break;
        default:
            supported = false;
        break;
    }

    PrintToLog("Feature activation of ID %d processed. %s will be enabled at block %d.\n", featureId, featureName, activationBlock);
    AddPendingActivation(featureId, activationBlock, minClientVersion, featureName);

    if (!supported) {
        PrintToLog("WARNING!!! AS OF BLOCK %d THIS CLIENT WILL BE OUT OF CONSENSUS AND WILL AUTOMATICALLY SHUTDOWN.\n", activationBlock);
        std::string alertText = strprintf("Your client must be updated and will shutdown at block %d (unsupported feature %d ('%s') activated)\n",
                                          activationBlock, featureId, featureName);
        AddAlert("omnicore", ALERT_BLOCK_EXPIRY, activationBlock, alertText);
        DoWarning(alertText);
    }

    return true;
}

/**
 * Deactivates a feature immediately, authorization has already been validated.
 *
 * Note: There is no notice period for feature deactivation as:
 *       # It is reserved for emergency use in the event an exploit is found
 *       # No client upgrade is required
 *       # No action is required by users
 */
bool DeactivateFeature(uint16_t featureId, int transactionBlock)
{
    PrintToLog("Immediate feature deactivation requested (ID %d)\n", featureId);

    if (!IsFeatureActivated(featureId, transactionBlock)) {
        PrintToLog("Feature deactivation of ID %d refused as the feature is not yet live\n", featureId);
        return false;
    }

    std::string featureName = GetFeatureName(featureId);
    switch (featureId) {
        case FEATURE_CLASS_C:
            MutableConsensusParams().NULLDATA_BLOCK = std::numeric_limits<int>::max();
        break;
        case FEATURE_METADEX:
            MutableConsensusParams().MSC_METADEX_BLOCK = std::numeric_limits<int>::max();
        break;
        case FEATURE_BETTING:
            MutableConsensusParams().MSC_BET_BLOCK = std::numeric_limits<int>::max();
        break;
        case FEATURE_GRANTEFFECTS:
            MutableConsensusParams().GRANTEFFECTS_FEATURE_BLOCK = std::numeric_limits<int>::max();
        break;
        case FEATURE_DEXMATH:
            MutableConsensusParams().DEXMATH_FEATURE_BLOCK = std::numeric_limits<int>::max();
        break;
        case FEATURE_SENDALL:
            MutableConsensusParams().MSC_SEND_ALL_BLOCK = std::numeric_limits<int>::max();
        break;
        case FEATURE_SPCROWDCROSSOVER:
            MutableConsensusParams().SPCROWDCROSSOVER_FEATURE_BLOCK = std::numeric_limits<int>::max();
        break;
        case FEATURE_TRADEALLPAIRS:
            MutableConsensusParams().TRADEALLPAIRS_FEATURE_BLOCK = std::numeric_limits<int>::max();
        break;
        case FEATURE_FEES:
            MutableConsensusParams().FEES_FEATURE_BLOCK = std::numeric_limits<int>::max();
        break;
        case FEATURE_STOV1:
            MutableConsensusParams().MSC_STOV1_BLOCK = std::numeric_limits<int>::max();
        break;
        case FEATURE_FREEZENOTICE:
            MutableConsensusParams().FREEZENOTICE_FEATURE_BLOCK = std::numeric_limits<int>::max();
        break;
        case FEATURE_FREEDEX:
            MutableConsensusParams().FREEDEX_FEATURE_BLOCK = std::numeric_limits<int>::max();
        break;
        case FEATURE_NONFUNGIBLE:
            MutableConsensusParams().MSC_NONFUNGIBLE_BLOCK = std::numeric_limits<int>::max();
        break;
        case FEATURE_NONFUNGIBLE_ISSUER:
            MutableConsensusParams().NONFUNGIBLETOKEN_ISSUER_DATA = std::numeric_limits<int>::max();
        break;
        case FEATURE_DELEGATEDISSUANCE:
            MutableConsensusParams().MSC_DELEGATED_ISSUANCE_BLOCK = std::numeric_limits<int>::max();
        break;
        case FEATURE_SEND_TO_MANY:
            MutableConsensusParams().MSC_SEND_TO_MANY_BLOCK = 999999;
        break;
        default:
            return false;
        break;
    }

    PrintToLog("Feature deactivation of ID %d processed. %s has been disabled.\n", featureId, featureName);

    std::string alertText = strprintf("An emergency deactivation of feature ID %d (%s) has occurred.", featureId, featureName);
    AddAlert("omnicore", ALERT_BLOCK_EXPIRY, transactionBlock + 1024, alertText);
    DoWarning(alertText);

    return true;
}

/**
 * Returns the display name of a feature ID
 */
std::string GetFeatureName(uint16_t featureId)
{
    switch (featureId) {
        case FEATURE_CLASS_C: return "Class C transaction encoding";
        case FEATURE_METADEX: return "Distributed Meta Token Exchange";
        case FEATURE_BETTING: return "Bet transactions";
        case FEATURE_GRANTEFFECTS: return "Remove grant side effects";
        case FEATURE_DEXMATH: return "DEx integer math update";
        case FEATURE_SENDALL: return "Send All transactions";
        case FEATURE_SPCROWDCROSSOVER: return "Disable crowdsale ecosystem crossovers";
        case FEATURE_TRADEALLPAIRS: return "Allow trading all pairs on the Distributed Exchange";
        case FEATURE_FEES: return "Fee system (inc 0.05% fee from trades of non-Omni pairs)";
        case FEATURE_STOV1: return "Cross-property Send To Owners";
        case FEATURE_XEP_CROWDSALES: return "Allow XEP as the desired property in a crowdsale)";
        case FEATURE_FREEZENOTICE: return "Activate the waiting period for enabling freezing";
        case FEATURE_FREEDEX: return "Activate trading of any token on the distributed exchange";
        case FEATURE_NONFUNGIBLE: return "Uniquely identifiable tokens";
        case FEATURE_NONFUNGIBLE_ISSUER: return "NFT issuer data update by issuers only";
        case FEATURE_DELEGATEDISSUANCE: return "Activate delegated issuance of tokens";
        case FEATURE_SEND_TO_MANY: return "Activate send-to-many transactions";

        default: return "Unknown feature";
    }
}

/**
 * Checks, whether a feature is activated at the given block.
 */
bool IsFeatureActivated(uint16_t featureId, int transactionBlock)
{
    const CConsensusParams& params = ConsensusParams();
    int activationBlock = std::numeric_limits<int>::max();

    switch (featureId) {
        case FEATURE_CLASS_C:
            activationBlock = params.NULLDATA_BLOCK;
            break;
        case FEATURE_METADEX:
            activationBlock = params.MSC_METADEX_BLOCK;
            break;
        case FEATURE_BETTING:
            activationBlock = params.MSC_BET_BLOCK;
            break;
        case FEATURE_GRANTEFFECTS:
            activationBlock = params.GRANTEFFECTS_FEATURE_BLOCK;
            break;
        case FEATURE_DEXMATH:
            activationBlock = params.DEXMATH_FEATURE_BLOCK;
            break;
        case FEATURE_SENDALL:
            activationBlock = params.MSC_SEND_ALL_BLOCK;
            break;
        case FEATURE_SPCROWDCROSSOVER:
            activationBlock = params.SPCROWDCROSSOVER_FEATURE_BLOCK;
            break;
        case FEATURE_TRADEALLPAIRS:
            activationBlock = params.TRADEALLPAIRS_FEATURE_BLOCK;
            break;
        case FEATURE_FEES:
            activationBlock = params.FEES_FEATURE_BLOCK;
            break;
        case FEATURE_STOV1:
            activationBlock = params.MSC_STOV1_BLOCK;
            break;
        case FEATURE_XEP_CROWDSALES:
            activationBlock = params.FEES_FEATURE_BLOCK;
            break;
        case FEATURE_FREEZENOTICE:
            activationBlock = params.FREEZENOTICE_FEATURE_BLOCK;
        break;
        case FEATURE_FREEDEX:
            activationBlock = params.FREEDEX_FEATURE_BLOCK;
        break;
        case FEATURE_NONFUNGIBLE:
            activationBlock = params.MSC_NONFUNGIBLE_BLOCK;
        break;
        case FEATURE_NONFUNGIBLE_ISSUER:
            activationBlock = params.NONFUNGIBLETOKEN_ISSUER_DATA;
            break;
        case FEATURE_DELEGATEDISSUANCE:
            activationBlock = params.MSC_DELEGATED_ISSUANCE_BLOCK;
        break;
        case FEATURE_SEND_TO_MANY:
            activationBlock = params.MSC_SEND_TO_MANY_BLOCK;
        break;
        default:
            return false;
    }

    return (transactionBlock >= activationBlock);
}

/**
 * Checks, if the transaction type and version is supported and enabled.
 *
 * In the test ecosystem, transactions, which are known to the client are allowed
 * without height restriction.
 *
 * Certain transactions use a property identifier of 0 (= XEP) as wildcard, which
 * must explicitly be allowed.
 */
bool IsTransactionTypeAllowed(int txBlock, uint32_t txProperty, uint16_t txType, uint16_t version)
{
    const std::vector<TransactionRestriction>& vTxRestrictions = ConsensusParams().GetRestrictions();

    for (std::vector<TransactionRestriction>::const_iterator it = vTxRestrictions.begin(); it != vTxRestrictions.end(); ++it)
    {
        const TransactionRestriction& entry = *it;
        if (entry.txType != txType || entry.txVersion != version) {
            continue;
        }
        // a property identifier of 0 (= XEP) may be used as wildcard
        if (OMNI_PROPERTY_XEP == txProperty && !entry.allowWildcard) {
            continue;
        }
        // transactions are not restricted in the test ecosystem
        if (isTestEcosystemProperty(txProperty)) {
            return true;
        }
        if (txBlock >= entry.activationBlock) {
            return true;
        }
    }

    return false;
}

/**
 * Compares a supplied block, block hash and consensus hash against a hardcoded list of checkpoints.
 */
bool VerifyCheckpoint(int block, const uint256& blockHash)
{
    // optimization; we only checkpoint every 10,000 blocks - skip any further work if block not a multiple of 10K
    if (block % 10000 != 0) return true;

    const std::vector<ConsensusCheckpoint>& vCheckpoints = ConsensusParams().GetCheckpoints();

    for (std::vector<ConsensusCheckpoint>::const_iterator it = vCheckpoints.begin(); it != vCheckpoints.end(); ++it) {
        const ConsensusCheckpoint& checkpoint = *it;
        if (block != checkpoint.blockHeight) {
            continue;
        }

        if (blockHash != checkpoint.blockHash) {
            PrintToLog("%s(): block hash mismatch - expected %s, received %s\n", __func__, checkpoint.blockHash.GetHex(), blockHash.GetHex());
            return false;
        }

        // only verify if there is a checkpoint to verify against
        uint256 consensusHash = GetConsensusHash();
        if (consensusHash != checkpoint.consensusHash) {
            PrintToLog("%s(): consensus hash mismatch - expected %s, received %s\n", __func__, checkpoint.consensusHash.GetHex(), consensusHash.GetHex());
            return false;
        } else {
            break;
        }
    }

    // either checkpoint matched or we don't have a checkpoint for this block
    return true;
}

/**
 * Checks, if a specific transaction exists in the database.
 */
bool VerifyTransactionExistence(int block)
{
    PrintToLog("%s: verifying existence of historical transactions up to block %d..\n", __func__, block);

    const std::vector<TransactionCheckpoint>& vTransactionss = ConsensusParams().GetTransactions();

    for (std::vector<TransactionCheckpoint>::const_iterator it = vTransactionss.begin(); it != vTransactionss.end(); ++it) {
        const TransactionCheckpoint& checkpoint = *it;
        if (block < checkpoint.blockHeight) {
            continue;
        }

        if (!mastercore::pDbTransactionList->exists(checkpoint.txHash)) {
            PrintToLog("%s: ERROR: failed to find historical transaction %s in block %d\n",
                    __func__, checkpoint.txHash.GetHex(), checkpoint.blockHeight);
            return false;
        }
    }

    return true;
}

} // namespace mastercore
