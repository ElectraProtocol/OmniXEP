// Copyright (c) 2018-2019 The Xep Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XEP_WALLET_TEST_INIT_TEST_FIXTURE_H
#define XEP_WALLET_TEST_INIT_TEST_FIXTURE_H

#include <interfaces/chain.h>
#include <node/context.h>
#include <test/util/setup_common.h>


struct InitWalletDirTestingSetup: public BasicTestingSetup {
    explicit InitWalletDirTestingSetup(const std::string& chainName = CBaseChainParams::MAIN);
    ~InitWalletDirTestingSetup();
    void SetWalletDir(const fs::path& walletdir_path);

    fs::path m_datadir;
    fs::path m_cwd;
    std::map<std::string, fs::path> m_walletdir_path_cases;
    NodeContext m_node;
    std::unique_ptr<interfaces::Chain> m_chain = interfaces::MakeChain(m_node);
    std::unique_ptr<interfaces::ChainClient> m_chain_client;
};

#endif // XEP_WALLET_TEST_INIT_TEST_FIXTURE_H
