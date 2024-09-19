// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2024, Stefan Hansson
 */

#ifndef __ZSTD_DECOMPRESS_H__
#define __ZSTD_DECOMPRESS_H__

#include <stdbool.h>

int zstd_decompress_file(const char *filename);

#endif
