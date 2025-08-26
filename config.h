// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <limits.h>
#include "logging.h"

/* Configuration structure for TQFTP server */
struct tqftp_config {
	/* Path configuration */
	char readonly_path[PATH_MAX];
	char readwrite_path[PATH_MAX];
	char firmware_base[PATH_MAX];
	char updates_dir[PATH_MAX];
	char temp_dir[PATH_MAX];

	/* Logging configuration */
	struct tqftp_log_config log_config;

	/* Configuration file path */
	char config_file[PATH_MAX];
};

/* Default configuration values */
#define DEFAULT_READONLY_PATH	"/readonly/firmware/image/"
#define DEFAULT_READWRITE_PATH	"/readwrite/"

#ifndef ANDROID
#define DEFAULT_FIRMWARE_BASE	"/lib/firmware/"
#define DEFAULT_TEMP_DIR	"/tmp/tqftpserv"
#define DEFAULT_UPDATES_DIR	"updates/"
#else
#define DEFAULT_FIRMWARE_BASE	"/vendor/firmware/"
#define DEFAULT_TEMP_DIR	"/data/vendor/tmp/tqftpserv"
#define DEFAULT_UPDATES_DIR	"updates/"
#endif

#define DEFAULT_CONFIG_FILE	"/etc/tqftpserv.conf"

/* Global configuration */
extern struct tqftp_config tqftp_config;

/* Function prototypes */
void tqftp_config_init_defaults(struct tqftp_config *config);
int tqftp_config_load_file(struct tqftp_config *config, const char *filename);
int tqftp_config_parse_args(int argc, char **argv, struct tqftp_config *config);
void tqftp_config_print_help(const char *progname);

#endif /* __CONFIG_H__ */
