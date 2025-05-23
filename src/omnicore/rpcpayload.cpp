#include <omnicore/createpayload.h>
#include <omnicore/nftdb.h>
#include <omnicore/rpcvalues.h>
#include <omnicore/rpcrequirements.h>
#include <omnicore/omnicore.h>
#include <omnicore/sp.h>
#include <omnicore/tx.h>

#include <rpc/server.h>
#include <rpc/util.h>
#include <util/strencodings.h>

#include <univalue.h>

#include <stdint.h>

using std::runtime_error;
using namespace mastercore;

static UniValue omni_createpayload_simplesend(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_simplesend",
       "\nCreate the payload for a simple send transaction.",
       {
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to send\n"},
           {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount to send\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_simplesend", "1 \"100.0\"")
           + HelpExampleRpc("omni_createpayload_simplesend", "1, \"100.0\"")
       }
    }.Check(request);

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_SimpleSend(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_sendall(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_sendall",
       "\nCreate the payload for a send all transaction.\n",
       {
           {"ecosystem", RPCArg::Type::NUM, RPCArg::Optional::NO, "the ecosystem of the tokens to send (1 for main ecosystem, 2 for test ecosystem)\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_sendall", "2")
           + HelpExampleRpc("omni_createpayload_sendall", "2")
       }
    }.Check(request);

    uint8_t ecosystem = ParseEcosystem(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_SendAll(ecosystem);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_sendnonfungible(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_sendnonfungible",
       "\nCreate the payload for a non-fungible send transaction.\n",
       {
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to send"},
           {"tokenstart", RPCArg::Type::NUM, RPCArg::Optional::NO, "the first token in the range to send"},
           {"tokenend", RPCArg::Type::NUM, RPCArg::Optional::NO, "the last token in the range to send"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_sendnonfungible", "70 1 1000")
           + HelpExampleRpc("omni_createpayload_sendnonfungible", "70, 1, 1000")
       }
    }.Check(request);

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t tokenStart = request.params[1].get_int64(); // non-fungible tokens are indivisible only
    int64_t tokenEnd =  request.params[2].get_int64();

    RequireSaneNonFungibleRange(tokenStart, tokenEnd);

    std::vector<unsigned char> payload = CreatePayload_SendNonFungible(propertyId, tokenStart, tokenEnd);

    return HexStr(payload.begin(), payload.end());
}


// sets data for a specific token
static UniValue omni_createpayload_setnonfungibledata(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_setnonfungibledata",
        "Create the payload for a non-fungible token set data transaction\n",
        {
            {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the property identifier"},
            {"tokenstart", RPCArg::Type::NUM, RPCArg::Optional::NO, "the first token in the range to set data on"},
            {"tokenend", RPCArg::Type::NUM, RPCArg::Optional::NO, "the last token in the range to set data on"},
            {"issuer", RPCArg::Type::BOOL, RPCArg::Optional::NO, "if true issuer data set, otherwise holder data set"},
            {"data", RPCArg::Type::STR, RPCArg::Optional::NO, "data set as in either issuer or holder fields"},
        },
        RPCResult{
            RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
        },
        RPCExamples{
            HelpExampleCli("omni_createpayload_setnonfungibledata", "70 50 60 true \"string data\"")
            + HelpExampleRpc("omni_createpayload_setnonfungibledata", "70, 50, 60, true, \"string data\"")
        }
    }.Check(request);

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    uint64_t tokenStart = request.params[1].get_int64();
    int64_t tokenEnd =  request.params[2].get_int64();
    bool issuer = request.params[3].isNum() ? (request.params[3].get_int() != 0) : request.params[3].get_bool();
    std::string data = ParseText(request.params[4]);

    RequireSaneNonFungibleRange(tokenStart, tokenEnd);

    std::vector<unsigned char> payload = CreatePayload_SetNonFungibleData(propertyId,  tokenStart, tokenEnd, issuer, data);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_sendtomany(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_sendtomany",
       "\nCreate the payload for a send-to-many transaction.\n"
       "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
       {
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to send"},
           {"mapping", RPCArg::Type::ARR, RPCArg::Optional::NO, "an array with output index \"output\" starting at 0 and the \"amount\" to send",
                {
                    {"", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                        {
                            {"output", RPCArg::Type::NUM, RPCArg::Optional::NO, "the output number"},
                            {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to send to this output"},
                        },
                    },
                },
            },
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_sendtomany", "1 '[{\"output\": 2, \"amount\": \"10.5\"}, {\"output\": 3, \"amount\": \"0.5\"}]'")
           + HelpExampleRpc("omni_createpayload_sendtomany", "1, '[{\"output\": 2, \"amount\": \"10.5\"}, {\"output\": 3, \"amount\": \"0.5\"}]'")
       }
    }.Check(request);

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    std::vector<std::tuple<uint8_t, uint64_t>> outputValues;

    for (unsigned int idx = 0; idx < request.params[1].size(); idx++) {
        const UniValue& input = request.params[1][idx];
        const UniValue& o = input.get_obj();

        const UniValue& uvOutput = find_value(o, "output");
        const UniValue& uvAmount = find_value(o, "amount");

        uint8_t output = ParseStmOutputIndex(uvOutput);
        uint64_t amount = ParseAmount(uvAmount, isPropertyDivisible(propertyId));

        outputValues.push_back(std::make_tuple(output, amount));
    }

    std::vector<unsigned char> payload = CreatePayload_SendToMany(
        propertyId,
        outputValues);           

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_dexsell(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_dexsell",
       "\nCreate a payload to place, update or cancel a sell offer on the traditional distributed OMNI/XEP exchange.\n",
       {
           {"propertyidforsale", RPCArg::Type::NUM, RPCArg::Optional::NO, " the identifier of the tokens to list for sale (must be 1 for OMN or 2 for TOMN)\n"},
           {"amountforsale", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to list for sale\n"},
           {"amountdesired", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of xeps desired\n"},
           {"paymentwindow", RPCArg::Type::NUM, RPCArg::Optional::NO, "a time limit in blocks a buyer has to pay following a successful accepting order\n"},
           {"minacceptfee", RPCArg::Type::STR, RPCArg::Optional::NO, "a minimum mining fee a buyer has to pay to accept the offer\n"},
           {"action", RPCArg::Type::NUM, RPCArg::Optional::NO, "the action to take (1 for new offers, 2 to update\", 3 to cancel)\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_dexsell", "1 \"1.5\" \"0.75\" 25 \"0.0005\" 1")
           + HelpExampleRpc("omni_createpayload_dexsell", "1, \"1.5\", \"0.75\", 25, \"0.0005\", 1")
       }
    }.Check(request);

    uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
    uint8_t action = ParseDExAction(request.params[5]);

    int64_t amountForSale = 0; // depending on action
    int64_t amountDesired = 0; // depending on action
    uint8_t paymentWindow = 0; // depending on action
    int64_t minAcceptFee = 0;  // depending on action

    if (action <= CMPTransaction::UPDATE) { // actions 3 permit zero values, skip check
        amountForSale = ParseAmount(request.params[1], isPropertyDivisible(propertyIdForSale));
        amountDesired = ParseAmount(request.params[2], true); // XEP is divisible
        paymentWindow = ParseDExPaymentWindow(request.params[3]);
        minAcceptFee = ParseDExFee(request.params[4]);
    }

    std::vector<unsigned char> payload = CreatePayload_DExSell(propertyIdForSale, amountForSale, amountDesired, paymentWindow, minAcceptFee, action);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_dexaccept(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_dexaccept",
       "\nCreate the payload for an accept offer for the specified token and amount.\n"
        "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
       {
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the token to purchase\n"},
           {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount to accept\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_dexaccept", "1 \"15.0\"")
           + HelpExampleRpc("omni_createpayload_dexaccept", "1, \"15.0\"")
       }
    }.Check(request);

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_DExAccept(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_sto(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_sto",
       "\nCreates the payload for a send-to-owners transaction.\n"
       "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
       {
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to distribute\n"},
           {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount to distribute\n"},
           {"distributionproperty", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "the identifier of the property holders to distribute to\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_sto", "3 \"5000\"")
           + HelpExampleRpc("omni_createpayload_sto", "3, \"5000\"")
       }
    }.Check(request);

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));
    uint32_t distributionPropertyId = (request.params.size() > 2) ? ParsePropertyId(request.params[2]) : propertyId;

    std::vector<unsigned char> payload = CreatePayload_SendToOwners(propertyId, amount, distributionPropertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_issuancefixed(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_issuancefixed",
       "\nCreates the payload for a new tokens issuance with fixed supply.\n",
       {
           {"ecosystem", RPCArg::Type::NUM, RPCArg::Optional::NO, "the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"},
           {"type", RPCArg::Type::NUM, RPCArg::Optional::NO, "the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"},
           {"previousid", RPCArg::Type::NUM, RPCArg::Optional::NO, "an identifier of a predecessor token (use 0 for new tokens)\n"},
           {"category", RPCArg::Type::STR, RPCArg::Optional::NO, "a category for the new tokens (can be \"\")\n"},
           {"subcategory", RPCArg::Type::STR, RPCArg::Optional::NO, "a subcategory for the new tokens  (can be \"\")\n"},
           {"name", RPCArg::Type::STR, RPCArg::Optional::NO, "the name of the new tokens to create\n"},
           {"url", RPCArg::Type::STR, RPCArg::Optional::NO, "a URL for further information about the new tokens (can be \"\")\n"},
           {"data", RPCArg::Type::STR, RPCArg::Optional::NO, "a description for the new tokens (can be \"\")\n"},
           {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the number of tokens to create\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_issuancefixed", "2 1 0 \"Companies\" \"Xep Mining\" \"Quantum Miner\" \"\" \"\" \"1000000\"")
           + HelpExampleRpc("omni_createpayload_issuancefixed", "2, 1, 0, \"Companies\", \"Xep Mining\", \"Quantum Miner\", \"\", \"\", \"1000000\"")
       }
    }.Check(request);

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string category = ParseText(request.params[3]);
    std::string subcategory = ParseText(request.params[4]);
    std::string name = ParseText(request.params[5]);
    std::string url = ParseText(request.params[6]);
    std::string data = ParseText(request.params[7]);
    int64_t amount = ParseAmount(request.params[8], type);

    RequirePropertyName(name);

    std::vector<unsigned char> payload = CreatePayload_IssuanceFixed(ecosystem, type, previousId, category, subcategory, name, url, data, amount);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_issuancecrowdsale(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_issuancecrowdsale",
       "\nCreates the payload for a new tokens issuance with crowdsale.\n",
       {
           {"ecosystem", RPCArg::Type::NUM, RPCArg::Optional::NO, "the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"},
           {"type", RPCArg::Type::NUM, RPCArg::Optional::NO, "the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"},
           {"previousid", RPCArg::Type::NUM, RPCArg::Optional::NO, "an identifier of a predecessor token (0 for new crowdsales)\n"},
           {"category", RPCArg::Type::STR, RPCArg::Optional::NO, "a category for the new tokens (can be \"\")\n"},
           {"subcategory", RPCArg::Type::STR, RPCArg::Optional::NO, "a subcategory for the new tokens  (can be \"\")\n"},
           {"name", RPCArg::Type::STR, RPCArg::Optional::NO, "the name of the new tokens to create\n"},
           {"url", RPCArg::Type::STR, RPCArg::Optional::NO, "a URL for further information about the new tokens (can be \"\")\n"},
           {"data", RPCArg::Type::STR, RPCArg::Optional::NO, "a description for the new tokens (can be \"\")\n"},
           {"propertyiddesired", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of a token eligible to participate in the crowdsale\n"},
           {"tokensperunit", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens granted per unit invested in the crowdsale\n"},
           {"deadline", RPCArg::Type::NUM, RPCArg::Optional::NO, "the deadline of the crowdsale as Unix timestamp\n"},
           {"earlybonus", RPCArg::Type::NUM, RPCArg::Optional::NO, "an early bird bonus for participants in percent per week\n"},
           {"issuerpercentage", RPCArg::Type::NUM, RPCArg::Optional::NO, "a percentage of tokens that will be granted to the issuer\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_issuancecrowdsale", "2 1 0 \"Companies\" \"Xep Mining\" \"Quantum Miner\" \"\" \"\" 2 \"100\" 1483228800 30 2")
           + HelpExampleRpc("omni_createpayload_issuancecrowdsale", "2, 1, 0, \"Companies\", \"Xep Mining\", \"Quantum Miner\", \"\", \"\", 2, \"100\", 1483228800, 30, 2")
       }
    }.Check(request);

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string category = ParseText(request.params[3]);
    std::string subcategory = ParseText(request.params[4]);
    std::string name = ParseText(request.params[5]);
    std::string url = ParseText(request.params[6]);
    std::string data = ParseText(request.params[7]);
    //uint32_t propertyIdDesired = ParsePropertyIdOrZero(request.params[8]);
    uint32_t propertyIdDesired = ParsePropertyId(request.params[8]);
    int64_t numTokens = ParseAmount(request.params[9], type);
    int64_t deadline = ParseDeadline(request.params[10]);
    uint8_t earlyBonus = ParseEarlyBirdBonus(request.params[11]);
    uint8_t issuerPercentage = ParseIssuerBonus(request.params[12]);

    //Line commented out because it prevents omni_getactivecrowdsales from populating and active crowdsales
    //RequirePropertyName(name);
    if (propertyIdDesired != XEP_PROPERTY_ID) {
        RequireExistingProperty(propertyIdDesired);
        RequireSameEcosystem(ecosystem, propertyIdDesired);
    }

    std::vector<unsigned char> payload = CreatePayload_IssuanceVariable(ecosystem, type, previousId, category, subcategory, name, url, data, propertyIdDesired, numTokens, deadline, earlyBonus, issuerPercentage);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_issuancemanaged(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_issuancemanaged",
       "\nCreates the payload for a new tokens issuance with manageable supply.\n",
       {
           {"ecosystem", RPCArg::Type::NUM, RPCArg::Optional::NO, "the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"},
           {"type", RPCArg::Type::NUM, RPCArg::Optional::NO, "the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens, 5 for non-fungible tokens)\n"},
           {"previousid", RPCArg::Type::NUM, RPCArg::Optional::NO, "an identifier of a predecessor token (use 0 for new tokens)\n"},
           {"category", RPCArg::Type::STR, RPCArg::Optional::NO, "a category for the new tokens (can be \"\")\n"},
           {"subcategory", RPCArg::Type::STR, RPCArg::Optional::NO, "a subcategory for the new tokens  (can be \"\")\n"},
           {"name", RPCArg::Type::STR, RPCArg::Optional::NO, "the name of the new tokens to create\n"},
           {"url", RPCArg::Type::STR, RPCArg::Optional::NO, "a URL for further information about the new tokens (can be \"\")\n"},
           {"data", RPCArg::Type::STR, RPCArg::Optional::NO, "a description for the new tokens (can be \"\")\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_issuancemanaged", "2 1 0 \"Companies\" \"Xep Mining\" \"Quantum Miner\" \"\" \"\"")
           + HelpExampleRpc("omni_createpayload_issuancemanaged", "2, 1, 0, \"Companies\", \"Xep Mining\", \"Quantum Miner\", \"\", \"\"")
       }
    }.Check(request);

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint16_t type = ParseManagedPropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string category = ParseText(request.params[3]);
    std::string subcategory = ParseText(request.params[4]);
    std::string name = ParseText(request.params[5]);
    std::string url = ParseText(request.params[6]);
    std::string data = ParseText(request.params[7]);

    RequirePropertyName(name);

    std::vector<unsigned char> payload = CreatePayload_IssuanceManaged(ecosystem, type, previousId, category, subcategory, name, url, data);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_closecrowdsale(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_closecrowdsale",
       "\nCreates the payload to manually close a crowdsale.\n",
       {
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the crowdsale to close\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_closecrowdsale", "70")
           + HelpExampleRpc("omni_createpayload_closecrowdsale", "70")
       }
    }.Check(request);

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_CloseCrowdsale(propertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_grant(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_grant",
       "\nCreates the payload to issue or grant new units of managed tokens.\n"
       "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
       {
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to grant\n"},
           {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to create\n"},
           {"grantdata", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "NFT only: data set in all NFTs created in this grant (default: empty)\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_grant", "51 \"7000\"")
           + HelpExampleRpc("omni_createpayload_grant", "51, \"7000\"")
       }
    }.Check(request);

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));
    std::string info = (request.params.size() > 2) ? ParseText(request.params[2]): "";

    std::vector<unsigned char> payload = CreatePayload_Grant(propertyId, amount, info);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_revoke(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_revoke",
       "\nCreates the payload to revoke units of managed tokens.\n"
       "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
       {
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to revoke\n"},
           {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to revoke\n"},
           {"memo", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "a text note attached to this transaction\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_revoke", "51 \"100\"")
           + HelpExampleRpc("omni_createpayload_revoke", "51, \"100\"")
       }
    }.Check(request);

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));
    std::string memo = (request.params.size() > 2) ? ParseText(request.params[2]): "";

    std::vector<unsigned char> payload = CreatePayload_Revoke(propertyId, amount, memo);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_changeissuer(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_changeissuer",
       "\nCreats the payload to change the issuer on record of the given tokens.\n",
       {
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_changeissuer", "3")
           + HelpExampleRpc("omni_createpayload_changeissuer", "3")
       }
    }.Check(request);

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_ChangeIssuer(propertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_trade(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_trade",
       "\nCreates the payload to place a trade offer on the distributed token exchange.\n"
       "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
       {
           {"propertyidforsale", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to list for sale\n"},
           {"amountforsale", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to list for sale\n"},
           {"propertiddesired", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens desired in exchange\n"},
           {"amountdesired", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens desired in exchange\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_trade", "31 \"250.0\" 1 \"10.0\"")
           + HelpExampleRpc("omni_createpayload_trade", "31, \"250.0\", 1, \"10.0\"")
       }
    }.Check(request);

    uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
    int64_t amountForSale = ParseAmount(request.params[1], isPropertyDivisible(propertyIdForSale));
    uint32_t propertyIdDesired = ParsePropertyId(request.params[2]);
    int64_t amountDesired = ParseAmount(request.params[3], isPropertyDivisible(propertyIdDesired));
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);

    std::vector<unsigned char> payload = CreatePayload_MetaDExTrade(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_canceltradesbyprice(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_canceltradesbyprice",
       "\nCreates the payload to cancel offers on the distributed token exchange with the specified price.\n"
       "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
       {
           {"propertyidforsale", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens listed for sale\n"},
           {"amountforsale", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to listed for sale\n"},
           {"propertiddesired", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens desired in exchange\n"},
           {"amountdesired", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens desired in exchange\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_canceltradesbyprice", "31 \"100.0\" 1 \"5.0\"")
           + HelpExampleRpc("omni_createpayload_canceltradesbyprice", "31, \"100.0\", 1, \"5.0\"")
       }
    }.Check(request);

    uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
    int64_t amountForSale = ParseAmount(request.params[1], isPropertyDivisible(propertyIdForSale));
    uint32_t propertyIdDesired = ParsePropertyId(request.params[2]);
    int64_t amountDesired = ParseAmount(request.params[3], isPropertyDivisible(propertyIdDesired));
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);

    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelPrice(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_canceltradesbypair(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_canceltradesbypair",
       "\nCreates the payload to cancel all offers on the distributed token exchange with the given currency pair.\n",
       {
           {"propertyidforsale", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens listed for sale\n"},
           {"propertiddesired", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens desired in exchange\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_canceltradesbypair", "1 31")
           + HelpExampleRpc("omni_createpayload_canceltradesbypair", "1, 31")
       }
    }.Check(request);

    uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
    uint32_t propertyIdDesired = ParsePropertyId(request.params[1]);
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);

    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelPair(propertyIdForSale, propertyIdDesired);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_cancelalltrades(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_cancelalltrades",
       "\nCreates the payload to cancel all offers on the distributed token exchange.\n",
       {
           {"ecosystem", RPCArg::Type::NUM, RPCArg::Optional::NO, "the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_cancelalltrades", "1")
           + HelpExampleRpc("omni_createpayload_cancelalltrades", "1")
       }
    }.Check(request);

    uint8_t ecosystem = ParseEcosystem(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelEcosystem(ecosystem);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_enablefreezing(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_enablefreezing",
       "\nCreates the payload to enable address freezing for a centrally managed property.\n",
       {
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_enablefreezing", "3")
           + HelpExampleRpc("omni_createpayload_enablefreezing", "3")
       }
    }.Check(request);

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_EnableFreezing(propertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_disablefreezing(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_disablefreezing",
       "\nCreates the payload to disable address freezing for a centrally managed property.\n"
       "\nIMPORTANT NOTE:  Disabling freezing for a property will UNFREEZE all frozen addresses for that property!",
       {
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_disablefreezing", "3")
           + HelpExampleRpc("omni_createpayload_disablefreezing", "3")
       }
    }.Check(request);

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_DisableFreezing(propertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_freeze(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_freeze",
       "\nCreates the payload to freeze an address for a centrally managed token.\n"
       "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
       {
           {"toaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to freeze tokens for\n"},
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the property to freeze tokens for (must be managed type and have freezing option enabled)\n"},
           {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to freeze (note: this is unused - once frozen an address cannot send any transactions)\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_freeze", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" 1 \"100\"")
           + HelpExampleRpc("omni_createpayload_freeze", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", 1, \"100\"")
       }
    }.Check(request);

    std::string refAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);
    int64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_FreezeTokens(propertyId, amount, refAddress);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_unfreeze(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_unfreeze",
       "\nCreates the payload to unfreeze an address for a centrally managed token.\n"
       "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
       {
           {"toaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to unfreeze tokens for\n"},
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the property to unfreeze tokens for (must be managed type and have freezing option enabled)\n"},
           {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to unfreeze (note: this is unused)\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_unfreeze", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" 1 \"100\"")
           + HelpExampleRpc("omni_createpayload_unfreeze", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", 1, \"100\"")
       }
    }.Check(request);

    std::string refAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);
    int64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_UnfreezeTokens(propertyId, amount, refAddress);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_adddelegate(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_adddelegate",
       "\nCreates the payload to add a delegate for the issuance of tokens of a managed property.\n",
       {
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_adddelegate", "21")
           + HelpExampleRpc("omni_createpayload_adddelegate", "21")
       }
    }.Check(request);

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_AddDelegate(propertyId);

    return HexStr(payload.begin(), payload.end());
}


static UniValue omni_createpayload_removedelegate(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_removedelegate",
       "\nCreates the payload to remove a delegate for the issuance of tokens of a managed property.\n",
       {
           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload",
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_removedelegate", "21")
           + HelpExampleRpc("omni_createpayload_removedelegate", "21")
       }
    }.Check(request);

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_RemoveDelegate(propertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_anydata(const JSONRPCRequest& request)
{
    RPCHelpMan{"omni_createpayload_anydata",
       "\nCreates the payload to embed arbitrary data.\n",
       {
           {"data", RPCArg::Type::STR, RPCArg::Optional::NO, "the hex-encoded data\n"},
       },
       RPCResult{
           RPCResult::Type::STR_HEX, "payload", "the hex-encoded payload"
       },
       RPCExamples{
           HelpExampleCli("omni_createpayload_anydata", "\"646578782032303230\"")
           + HelpExampleRpc("omni_createpayload_anydata", "\"646578782032303230\"")
       }
    }.Check(request);

    std::vector<unsigned char> data = ParseHexV(request.params[0], "data");

    std::vector<unsigned char> payload = CreatePayload_AnyData(data);

    return HexStr(payload.begin(), payload.end());
}

static const CRPCCommand commands[] =
{ //  category                         name                                      actor (function)                         okSafeMode
  //  -------------------------------- ----------------------------------------- ---------------------------------------- ----------
    { "omni layer (payload creation)", "omni_createpayload_simplesend",          &omni_createpayload_simplesend,          {"propertyid", "amount"} },
    { "omni layer (payload creation)", "omni_createpayload_sendall",             &omni_createpayload_sendall,             {"ecosystem"} },
    { "omni layer (payload creation)", "omni_createpayload_sendtomany",          &omni_createpayload_sendtomany,          {"propertyid", "mapping"} },
    { "omni layer (payload creation)", "omni_createpayload_dexsell",             &omni_createpayload_dexsell,             {"propertyidforsale", "amountforsale", "amountdesired", "paymentwindow", "minacceptfee", "action"} },
    { "omni layer (payload creation)", "omni_createpayload_dexaccept",           &omni_createpayload_dexaccept,           {"propertyid", "amount"} },
    { "omni layer (payload creation)", "omni_createpayload_sto",                 &omni_createpayload_sto,                 {"propertyid", "amount", "distributionproperty"} },
    { "omni layer (payload creation)", "omni_createpayload_grant",               &omni_createpayload_grant,               {"propertyid", "amount", "memo"} },
    { "omni layer (payload creation)", "omni_createpayload_revoke",              &omni_createpayload_revoke,              {"propertyid", "amount", "memo"} },
    { "omni layer (payload creation)", "omni_createpayload_changeissuer",        &omni_createpayload_changeissuer,        {"propertyid"} },
    { "omni layer (payload creation)", "omni_createpayload_trade",               &omni_createpayload_trade,               {"propertyidforsale", "amountforsale", "propertiddesired", "amountdesired"} },
    { "omni layer (payload creation)", "omni_createpayload_issuancefixed",       &omni_createpayload_issuancefixed,       {"ecosystem", "type", "previousid", "category", "subcategory", "name", "url", "data", "amount"} },
    { "omni layer (payload creation)", "omni_createpayload_issuancecrowdsale",   &omni_createpayload_issuancecrowdsale,   {"ecosystem", "type", "previousid", "category", "subcategory", "name", "url", "data", "propertyiddesired", "tokensperunit", "deadline", "earlybonus", "issuerpercentage"} },
    { "omni layer (payload creation)", "omni_createpayload_issuancemanaged",     &omni_createpayload_issuancemanaged,     {"ecosystem", "type", "previousid", "category", "subcategory", "name", "url", "data"} },
    { "omni layer (payload creation)", "omni_createpayload_closecrowdsale",      &omni_createpayload_closecrowdsale,      {"propertyid"} },
    { "omni layer (payload creation)", "omni_createpayload_canceltradesbyprice", &omni_createpayload_canceltradesbyprice, {"propertyidforsale", "amountforsale", "propertiddesired", "amountdesired"} },
    { "omni layer (payload creation)", "omni_createpayload_canceltradesbypair",  &omni_createpayload_canceltradesbypair,  {"propertyidforsale", "propertiddesired"} },
    { "omni layer (payload creation)", "omni_createpayload_cancelalltrades",     &omni_createpayload_cancelalltrades,     {"ecosystem"} },
    { "omni layer (payload creation)", "omni_createpayload_enablefreezing",      &omni_createpayload_enablefreezing,      {"propertyid"} },
    { "omni layer (payload creation)", "omni_createpayload_disablefreezing",     &omni_createpayload_disablefreezing,     {"propertyid"} },
    { "omni layer (payload creation)", "omni_createpayload_freeze",              &omni_createpayload_freeze,              {"toaddress", "propertyid", "amount"} },
    { "omni layer (payload creation)", "omni_createpayload_unfreeze",            &omni_createpayload_unfreeze,            {"toaddress", "propertyid", "amount"} },
    { "omni layer (payload creation)", "omni_createpayload_adddelegate",         &omni_createpayload_adddelegate,         {"propertyid"} },
    { "omni layer (payload creation)", "omni_createpayload_removedelegate",      &omni_createpayload_removedelegate,      {"propertyid"} },
    { "omni layer (payload creation)", "omni_createpayload_anydata",             &omni_createpayload_anydata,             {"data"} },
    { "omni layer (payload creation)", "omni_createpayload_sendnonfungible",     &omni_createpayload_sendnonfungible,     {"propertyid", "tokenstart", "tokenend"} },
    { "omni layer (payload creation)", "omni_createpayload_setnonfungibledata",  &omni_createpayload_setnonfungibledata,  {"propertyid", "tokenstart", "tokenend", "issuer", "data"} },
};

void RegisterOmniPayloadCreationRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
