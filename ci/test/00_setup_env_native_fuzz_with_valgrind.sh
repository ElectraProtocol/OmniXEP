#!/usr/bin/env bash
#
# Copyright (c) 2019 The Xep Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export DOCKER_NAME_TAG="ubuntu:20.04"
export CONTAINER_NAME=ci_native_fuzz_valgrind
export PACKAGES="clang llvm python3 libevent-dev bsdmainutils libboost-system-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev valgrind"
export NO_DEPENDS=1
export RUN_UNIT_TESTS=false
export RUN_FUNCTIONAL_TESTS=false
export RUN_FUZZ_TESTS=true
export FUZZ_TESTS_CONFIG="--valgrind"
export GOAL="install"
export XEP_CONFIG="--enable-fuzz --with-sanitizers=fuzzer CC=clang CXX=clang++"
