// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __XZ_DECOMPRESS_H__
#define __XZ_DECOMPRESS_H__

#include <stdbool.h>
#include <stdio.h>

#ifdef HAVE_XZ
int xz_decompress_file(const char *filename);
#else
static int xz_decompress_file(const char *filename)
{
	fprintf(stderr, "Built without XZ support\n");
	return -1;
}
#endif

#endif
