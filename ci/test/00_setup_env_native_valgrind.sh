#!/usr/bin/env bash
#
# Copyright (c) 2019 The Xep Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CONTAINER_NAME=ci_native_valgrind
export PACKAGES="valgrind clang llvm python3-zmq libevent-dev bsdmainutils libboost-system-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev libdb5.3++-dev libminiupnpc-dev libzmq3-dev"
export USE_VALGRIND=1
export NO_DEPENDS=1
export TEST_RUNNER_EXTRA="--exclude rpc_bind"  # Excluded for now, see https://github.com/xep/xep/issues/17765#issuecomment-602068547
export GOAL="install"
export XEP_CONFIG="--enable-zmq --with-incompatible-bdb --with-gui=no CC=clang CXX=clang++"  # TODO enable GUI
