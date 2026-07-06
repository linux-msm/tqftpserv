#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# Install build dependencies on Arch Linux.
#
# qrtr is only packaged in the AUR, which is not available in the CI
# container, so build and install it from source.

set -ex

PKGS_CC="gcc"
case $CC in
	clang*)
		PKGS_CC="clang"
	;;
esac

# zstd and xz (liblzma) ship as part of the Arch base image and cannot be
# removed, so there is no "disabled" variant to install here.
pacman -Syu --noconfirm \
	pkgconf \
	meson \
	ninja \
	git \
	zstd \
	xz \
	$PKGS_CC

QRTR_SRC=$(mktemp -d)
git clone --depth 1 https://github.com/linux-msm/qrtr "$QRTR_SRC"
meson setup --prefix=/usr "$QRTR_SRC" "$QRTR_SRC/build"
ninja -C "$QRTR_SRC/build"
ninja -C "$QRTR_SRC/build" install
rm -rf "$QRTR_SRC"

echo "Install finished: $0"
