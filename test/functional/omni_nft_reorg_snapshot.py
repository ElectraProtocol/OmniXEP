#!/usr/bin/env python3
# Copyright (c) 2017-2018 The Xep Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test NFT recovery across a reorg when the persisted snapshot lags the tip."""

import os

from test_framework.test_framework import XepTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    disconnect_nodes,
    wait_until,
)


class OmniNFTReorgSnapshotTest(XepTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [['-omniactivationallowsender=any'], ['-omniactivationallowsender=any']]

    def _persist_dir(self, node):
        return os.path.join(node.datadir, self.chain, "MP_persist")

    def _snapshot_heights(self, node):
        heights = []
        persist_dir = self._persist_dir(node)
        if not os.path.isdir(persist_dir):
            return heights

        for filename in os.listdir(persist_dir):
            if not filename.startswith("balances-") or not filename.endswith(".dat"):
                continue
            blockhash = filename[len("balances-"):-len(".dat")]
            heights.append(node.getblockheader(blockhash)["height"])

        return sorted(heights)

    def _wait_for_snapshot(self, node):
        wait_until(lambda: len(self._snapshot_heights(node)) > 0, timeout=30)

    def run_test(self):
        node0 = self.nodes[0]
        node1 = self.nodes[1]

        issuer = node0.getnewaddress()
        alt_miner = node1.getnewaddress()

        self.log.info("Mine enough blocks to mature funds and create the initial persisted snapshot")
        node0.generatetoaddress(110, issuer)
        self.sync_all()
        self._wait_for_snapshot(node0)

        snapshot_height = self._snapshot_heights(node0)[-1]
        self.log.info("Latest snapshot on node0 is at height %d", snapshot_height)

        self.log.info("Create shared NFT history after the snapshot so a later reorg has to replay it")
        creation_tx = node0.omni_sendissuancemanaged(issuer, 2, 5, 0, "", "", "REORGNFT", "", "")
        node0.generatetoaddress(1, issuer)
        self.sync_all()

        property_id = node0.omni_gettransaction(creation_tx)["propertyid"]
        creation_height = node0.getblockcount()
        if snapshot_height >= creation_height:
            raise AssertionError("test requires the latest snapshot to predate the NFT history")

        grant_a = node0.omni_sendgrant(issuer, "", property_id, "50", "shared grant A")
        node0.generatetoaddress(1, issuer)
        grant_b = node0.omni_sendgrant(issuer, "", property_id, "15", "shared grant B")
        node0.generatetoaddress(1, issuer)
        self.sync_all()

        assert_equal(node0.omni_gettransaction(grant_a)["valid"], True)
        assert_equal(node0.omni_gettransaction(grant_b)["valid"], True)

        ranges = node0.omni_getnonfungibletokenranges(property_id)
        assert_equal(len(ranges), 1)
        assert_equal(ranges[0]["address"], issuer)
        assert_equal(ranges[0]["tokenstart"], 1)
        assert_equal(ranges[0]["tokenend"], 65)
        assert_equal(ranges[0]["amount"], 65)

        token_data = node0.omni_getnonfungibletokendata(property_id, 65)
        assert_equal(token_data[0]["owner"], issuer)
        assert_equal(token_data[0]["grantdata"], "shared grant B")

        self.log.info("Force a live reorg after the shared NFT history")
        disconnect_nodes(node0, 1)
        disconnect_nodes(node1, 0)

        node0.generatetoaddress(1, issuer)
        node1.generatetoaddress(2, alt_miner)

        connect_nodes(node0, 1)
        self.sync_blocks()

        assert_equal(node0.getbestblockhash(), node1.getbestblockhash())
        assert_equal(node0.getblockcount(), node1.getblockcount())

        self.log.info("Grant another 7 NFTs after the reorg to prove the recovered state stayed aligned")
        grant_c = node0.omni_sendgrant(issuer, "", property_id, "7", "post reorg grant")
        node0.generatetoaddress(1, issuer)
        self.sync_all()

        grant_c_tx = node0.omni_gettransaction(grant_c)
        assert_equal(grant_c_tx["valid"], True)
        assert_equal(grant_c_tx["tokenstart"], "66")
        assert_equal(grant_c_tx["tokenend"], "72")

        ranges = node0.omni_getnonfungibletokenranges(property_id)
        assert_equal(len(ranges), 1)
        assert_equal(ranges[0]["address"], issuer)
        assert_equal(ranges[0]["tokenstart"], 1)
        assert_equal(ranges[0]["tokenend"], 72)
        assert_equal(ranges[0]["amount"], 72)

        node1_ranges = node1.omni_getnonfungibletokenranges(property_id)
        assert_equal(node1_ranges, ranges)

        token_data = node0.omni_getnonfungibletokendata(property_id, 72)
        assert_equal(token_data[0]["owner"], issuer)
        assert_equal(token_data[0]["grantdata"], "post reorg grant")


if __name__ == '__main__':
    OmniNFTReorgSnapshotTest().main()
