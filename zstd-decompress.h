// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2024, Stefan Hansson
 */

#ifndef __ZSTD_DECOMPRESS_H__
#define __ZSTD_DECOMPRESS_H__

#include <stdbool.h>

#ifdef HAVE_ZSTD
int zstd_decompress_file(const char *filename);
#else
static int zstd_decompress_file(const char *filename)
{
	fprintf(stderr, "Built without ZSTD support\n");
	return -1;
}
#endif

#endif
