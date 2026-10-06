#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# Verify that zstd support was autodetected as expected: zstd-decompress.c is
# only compiled when libzstd is available.

set -eu

OBJ=build/tqftpserv.p/zstd-decompress.c.o

case "$ZSTD" in
enabled)
	if [ ! -f "$OBJ" ]; then
		echo "ERROR: expected zstd support, but zstd-decompress.c was not built"
		exit 1
	fi
	;;
disabled)
	if [ -f "$OBJ" ]; then
		echo "ERROR: expected no zstd support, but zstd-decompress.c was built"
		exit 1
	fi
	;;
*)
	echo "ERROR: unexpected ZSTD value '$ZSTD'"
	exit 1
	;;
esac

echo "zstd support check ($ZSTD) passed"
