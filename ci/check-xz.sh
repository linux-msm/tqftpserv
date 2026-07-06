#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# Verify that xz support was autodetected as expected: xz-decompress.c is
# only compiled when liblzma is available.

set -eu

OBJ=build/tqftpserv.p/xz-decompress.c.o

case "$XZ" in
enabled)
	if [ ! -f "$OBJ" ]; then
		echo "ERROR: expected xz support, but xz-decompress.c was not built"
		exit 1
	fi
	;;
disabled)
	if [ -f "$OBJ" ]; then
		echo "ERROR: expected no xz support, but xz-decompress.c was built"
		exit 1
	fi
	;;
*)
	echo "ERROR: unexpected XZ value '$XZ'"
	exit 1
	;;
esac

echo "xz support check ($XZ) passed"
