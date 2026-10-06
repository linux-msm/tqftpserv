#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# Install build dependencies on Alpine Linux.

set -ex

# build-base pulls in binutils (the linker/assembler) and the musl headers,
# which clang needs as well; add clang on top when it is the compiler.
PKGS_CC=""
case $CC in
	clang*)
		PKGS_CC="clang"
	;;
esac

PKGS_ZSTD=""
if [ "$ZSTD" = "enabled" ]; then
	PKGS_ZSTD="zstd-dev"
fi

PKGS_XZ=""
if [ "$XZ" = "enabled" ]; then
	PKGS_XZ="xz-dev"
fi

apk add \
	build-base \
	pkgconf \
	meson \
	ninja \
	qrtr-dev \
	$PKGS_ZSTD \
	$PKGS_XZ \
	$PKGS_CC

echo "Install finished: $0"
