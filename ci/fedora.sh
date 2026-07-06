#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# Install build dependencies on Fedora.

set -ex

PKGS_CC="gcc"
case $CC in
	clang*)
		PKGS_CC="clang"
	;;
esac

PKGS_ZSTD=""
if [ "$ZSTD" = "enabled" ]; then
	PKGS_ZSTD="libzstd-devel"
fi

PKGS_XZ=""
if [ "$XZ" = "enabled" ]; then
	PKGS_XZ="xz-devel"
fi

dnf -y install \
	pkgconf-pkg-config \
	meson \
	ninja-build \
	qrtr-devel \
	$PKGS_ZSTD \
	$PKGS_XZ \
	$PKGS_CC

echo "Install finished: $0"
