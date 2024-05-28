// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2024, Stefan Hansson
 */

/* For memfd_create */
#define _GNU_SOURCE

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zstd.h>

#include "zstd-decompress.h"

static ZSTD_DCtx *zstd_context = NULL;

/**
 * zstd_init() - set up state for decompression. Needs to be called before zstd_decompress_file()
 */
void zstd_init()
{
	zstd_context = ZSTD_createDCtx();
}

/**
 * zstd_free() - free state used for decompression. zstd_decompress_file() may not be called after this
 */
void zstd_free()
{
	ZSTD_freeDCtx(zstd_context);
}

/**
 * zstd_decompress_file() - decompress a zstd-compressed file
 * @filename:	path to a file to decompress
 *
 * Return: opened fd on success, -1 on error
 */
int zstd_decompress_file(const char *filename)
{
	/* Figure out the size of the file. */
	struct stat file_stat;
	if (stat(filename, &file_stat) == -1) {
		perror("stat failed");
		return -1;
	}

	const size_t file_size = file_stat.st_size;

	const int input_file_fd = open(filename, 0);
	if (input_file_fd == -1) {
		perror("open failed");
		return -1;
	}

	void* const compressed_buffer = mmap(NULL, file_size, PROT_READ, MAP_POPULATE | MAP_PRIVATE, input_file_fd, 0);
	if (compressed_buffer == MAP_FAILED) {
		perror("mmap failed");
		close(input_file_fd);
		return -1;
	}
	close(input_file_fd);

	const unsigned long long decompressed_size = ZSTD_getFrameContentSize(compressed_buffer, file_size);
	if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
		fprintf(stderr, "Content size could not be determined for %s\n", filename);
		return -1;
	}
	if (decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
		fprintf(stderr, "Error getting content size for %s\n", filename);
		return -1;
	}

	void* const decompressed_buffer = malloc((size_t)decompressed_size);
	if (decompressed_buffer == NULL) {
		perror("malloc failed");
		return -1;
	}

	const size_t return_size = ZSTD_decompressDCtx(zstd_context, decompressed_buffer, decompressed_size, compressed_buffer, file_size);
	if (ZSTD_isError(return_size)) {
		fprintf(stderr, "ZSTD_decompress failed: %s\n", ZSTD_getErrorName(return_size));
		return -1;
	}

	const int output_file_fd = memfd_create(filename, 0);
	if (output_file_fd == -1) {
		perror("memfd_create failed");
		return -1;
	}

	if (write(output_file_fd, decompressed_buffer, decompressed_size) != decompressed_size) {
		perror("write failed");
		close(output_file_fd);
		return -1;
	}

	return output_file_fd;
}
