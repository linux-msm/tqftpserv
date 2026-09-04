#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# Install build dependencies on Debian and Ubuntu.

set -ex

apt update

# Some distros might pull tzdata which asks questions
export DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true

PKGS_CC="build-essential"
case $CC in
	clang*)
		PKGS_CC="clang"
	;;
esac

PKGS_ZSTD=""
if [ "$ZSTD" = "enabled" ]; then
	PKGS_ZSTD="libzstd-dev"
fi

apt install -y --no-install-recommends \
	pkg-config \
	meson \
	ninja-build \
	libqrtr-dev \
	$PKGS_ZSTD \
	$PKGS_CC

echo "Install finished: $0"
