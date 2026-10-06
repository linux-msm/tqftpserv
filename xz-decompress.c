// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * Based on the code copyright by:
 * Copyright (c) 2024, Stefan Hansson
 * Copyright (c) 2024, Emil Velikov
 */

/* For memfd_create */
#define _GNU_SOURCE

#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <lzma.h>

#include "xz-decompress.h"

/**
 * xz_decompress_file() - decompress an xz-compressed file
 * @filename:	path to a file to decompress
 *
 * The xz format does not store the uncompressed size in an easily
 * queryable header, so decode the stream incrementally and write the
 * output to a memfd as it is produced.
 *
 * Return: opened fd on success, -1 on error
 */
int xz_decompress_file(const char *filename)
{
	/* Figure out the size of the file. */
	struct stat file_stat;
	if (stat(filename, &file_stat) == -1) {
		fprintf(stderr, "stat %s failed (%s)\n", filename, strerror(errno));
		return -1;
	}

	const size_t file_size = file_stat.st_size;

	const int input_file_fd = open(filename, O_RDONLY);
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

	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, 0);
	if (ret != LZMA_OK) {
		fprintf(stderr, "lzma_stream_decoder failed for %s (error %d)\n", filename, ret);
		munmap(compressed_buffer, file_size);
		return -1;
	}

	const int output_file_fd = memfd_create(filename, 0);
	if (output_file_fd == -1) {
		perror("memfd_create failed");
		lzma_end(&strm);
		munmap(compressed_buffer, file_size);
		return -1;
	}

	uint8_t output_buffer[BUFSIZ];

	strm.next_in = compressed_buffer;
	strm.avail_in = file_size;

	do {
		strm.next_out = output_buffer;
		strm.avail_out = sizeof(output_buffer);

		ret = lzma_code(&strm, LZMA_FINISH);
		if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
			fprintf(stderr, "lzma_code failed for %s (error %d)\n", filename, ret);
			close(output_file_fd);
			lzma_end(&strm);
			munmap(compressed_buffer, file_size);
			return -1;
		}

		const size_t produced = sizeof(output_buffer) - strm.avail_out;
		if (write(output_file_fd, output_buffer, produced) != (ssize_t)produced) {
			perror("write failed");
			close(output_file_fd);
			lzma_end(&strm);
			munmap(compressed_buffer, file_size);
			return -1;
		}
	} while (ret != LZMA_STREAM_END);

	lzma_end(&strm);
	munmap(compressed_buffer, file_size);

	return output_file_fd;
}
