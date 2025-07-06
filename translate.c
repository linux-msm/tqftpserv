// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2019, Linaro Ltd.
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "translate.h"
#include "zstd-decompress.h"

#define READONLY_PATH	"/readonly/firmware/image/"
#define READWRITE_PATH	"/readwrite/"

#ifndef ANDROID
#define FIRMWARE_BASE	"/lib/firmware/"
#define TQFTPSERV_TMP	"/tmp/tqftpserv"
#define UPDATES_DIR	"updates/"
#else
#define FIRMWARE_BASE	"/vendor/firmware/"
#define TQFTPSERV_TMP	"/data/vendor/tmp/tqftpserv"
#endif

static int open_maybe_compressed(const char *path);

static void read_fw_path_from_sysfs(char *outbuffer, size_t bufsize)
{
	size_t pathsize;
	FILE *f = fopen("/sys/module/firmware_class/parameters/path", "rt");
	if (!f)
		return;
	pathsize = fread(outbuffer, sizeof(char), bufsize, f);
	fclose(f);
	if (pathsize == 0)
		return;
	/* truncate newline */
	outbuffer[pathsize - 1] = '\0';
}

/**
 * translate_readonly() - open "file" residing with remoteproc firmware
 * @file:	file requested, stripped of "/readonly/image/" prefix
 *
 * It is assumed that the readonly files requested by the client resides under
 * /lib/firmware in the same place as its associated remoteproc firmware.  This
 * function scans through all entries under /sys/class/remoteproc and read the
 * dirname of each "firmware" file in an attempt to find, and open(2), the
 * requested file.
 *
 * As these files are readonly, it's not possible to pass flags to open(2).
 *
 * Return: opened fd on success, -1 otherwise
 */
static int translate_readonly(const char *file)
{
	char firmware_value[PATH_MAX];
	char *firmware_value_copy = NULL;
	char *firmware_path;
	char firmware_attr[32];
	char path[PATH_MAX];
	char fw_sysfs_path[PATH_MAX];
	struct dirent *de;
	int firmware_fd;
	DIR *class_dir;
	int class_fd;
	ssize_t n;
	int fd = -1;

	read_fw_path_from_sysfs(fw_sysfs_path, sizeof(fw_sysfs_path));

	class_fd = open("/sys/class/remoteproc", O_RDONLY | O_DIRECTORY);
	if (class_fd < 0) {
		warn("failed to open remoteproc class");
		return -1;
	}

	class_dir = fdopendir(class_fd);
	if (!class_dir) {
		warn("failed to opendir");
		close(class_fd);
		return -1;
	}

	while ((de = readdir(class_dir)) != NULL) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;

		if (strlen(de->d_name) + sizeof("/firmware") > sizeof(firmware_attr))
			continue;
		strcpy(firmware_attr, de->d_name);
		strcat(firmware_attr, "/firmware");

		firmware_fd = openat(class_fd, firmware_attr, O_RDONLY);
		if (firmware_fd < 0)
			continue;

		n = read(firmware_fd, firmware_value, sizeof(firmware_value));
		close(firmware_fd);
		if (n < 0) {
			continue;
		}
		firmware_value[n] = '\0';

		firmware_value_copy = strdup(firmware_value);
		firmware_path = dirname(firmware_value_copy);

		/* first try path from sysfs */
		if ((strlen(fw_sysfs_path) > 0) &&
		    (strlen(fw_sysfs_path) + 1 + strlen(firmware_value) + 1 + strlen(file) + 1 < sizeof(path))) {
			strcpy(path, fw_sysfs_path);
			strcat(path, "/");
			strcat(path, firmware_path);
			strcat(path, "/");
			strcat(path, file);

			fd = open_maybe_compressed(path);
			if (fd >= 0)
				break;
			if (errno != ENOENT)
				warn("failed to open %s", path);
		}

		/* now try with base path */
		if (strlen(FIRMWARE_BASE) + strlen(UPDATES_DIR) + strlen(firmware_value) + 1 +
		    strlen(file) + 1 > sizeof(path))
			continue;

		strcpy(path, FIRMWARE_BASE);
		strcat(path, UPDATES_DIR);
		strcat(path, firmware_path);
		strcat(path, "/");
		strcat(path, file);

		fd = open_maybe_compressed(path);
		if (fd < 0) {
			strcpy(path, FIRMWARE_BASE);
			strcat(path, firmware_path);
			strcat(path, "/");
			strcat(path, file);

			fd = open_maybe_compressed(path);
		}
		if (fd >= 0)
			break;

		if (errno != ENOENT)
			warn("failed to open %s", path);
	}

	free(firmware_value_copy);
	closedir(class_dir);

	return fd;
}

/**
 * translate_readwrite() - open "file" from a temporary directory
 * @file:	relative path of the requested file, with /readwrite/ stripped
 * @flags:	flags to be passed to open(2)
 *
 * Return: opened fd on success, -1 otherwise
 */
static int translate_readwrite(const char *file, int flags)
{
	int base;
	int ret;
	int fd;

	ret = mkdir(TQFTPSERV_TMP, 0700);
	if (ret < 0 && errno != EEXIST) {
		warn("failed to create temporary tqftpserv directory");
		return -1;
	}

	base = open(TQFTPSERV_TMP, O_RDONLY | O_DIRECTORY);
	if (base < 0) {
		warn("failed top open temporary tqftpserv directory");
		return -1;
	}

	fd = openat(base, file, flags, 0600);
	close(base);
	if (fd < 0)
		warn("failed to open %s", file);

	return fd;
}

/**
 * translate_open() - open file after translating path
 *

 * Strips /readonly/firmware/image and search among remoteproc firmware.
 * Replaces /readwrite with a temporary directory.

 */
int translate_open(const char *path, int flags)
{
	if (!strncmp(path, READONLY_PATH, strlen(READONLY_PATH)))
		return translate_readonly(path + strlen(READONLY_PATH));
	else if (!strncmp(path, READWRITE_PATH, strlen(READWRITE_PATH)))
		return translate_readwrite(path + strlen(READWRITE_PATH), flags);

	fprintf(stderr, "invalid path %s, rejecting\n", path);
	errno = ENOENT;
	return -1;
}

/* linux-firmware uses .zst as file extension */
#define ZSTD_EXTENSION ".zst"

/**
 * open_maybe_compressed() - open a file and maybe decompress it if necessary
 * @filename:	path to a file that may be compressed (should not include compression format extension)
 *
 * Return: opened fd on success, -1 on error
 */
static int open_maybe_compressed(const char *path)
{
	char *path_with_zstd_extension = NULL;
	int fd = -1;

	if (access(path, F_OK) == 0)
		return open(path, O_RDONLY);

	asprintf(&path_with_zstd_extension, "%s%s", path, ZSTD_EXTENSION);

	if (access(path_with_zstd_extension, F_OK) == 0)
		fd = zstd_decompress_file(path_with_zstd_extension);

	free(path_with_zstd_extension);

	return fd;
}
