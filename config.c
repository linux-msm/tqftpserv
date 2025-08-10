// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <libconfig.h>
#include "config.h"
#include "logging.h"

/* Global configuration instance */
struct tqftp_config tqftp_config;

/**
 * tqftp_config_init_defaults() - Initialize configuration with default values
 * @config: Configuration structure to initialize
 */
void tqftp_config_init_defaults(struct tqftp_config *config)
{
	/* Initialize path configuration */
	strncpy(config->readonly_path, DEFAULT_READONLY_PATH, sizeof(config->readonly_path) - 1);
	strncpy(config->readwrite_path, DEFAULT_READWRITE_PATH, sizeof(config->readwrite_path) - 1);
	strncpy(config->firmware_base, DEFAULT_FIRMWARE_BASE, sizeof(config->firmware_base) - 1);
	strncpy(config->updates_dir, DEFAULT_UPDATES_DIR, sizeof(config->updates_dir) - 1);
	strncpy(config->temp_dir, DEFAULT_TEMP_DIR, sizeof(config->temp_dir) - 1);
	strncpy(config->config_file, DEFAULT_CONFIG_FILE, sizeof(config->config_file) - 1);

	/* Null terminate all strings */
	config->readonly_path[sizeof(config->readonly_path) - 1] = '\0';
	config->readwrite_path[sizeof(config->readwrite_path) - 1] = '\0';
	config->firmware_base[sizeof(config->firmware_base) - 1] = '\0';
	config->updates_dir[sizeof(config->updates_dir) - 1] = '\0';
	config->temp_dir[sizeof(config->temp_dir) - 1] = '\0';
	config->config_file[sizeof(config->config_file) - 1] = '\0';

	/* Initialize logging configuration - always use LOG_DAEMON as per @lumag's feedback */
	config->log_config.facility = LOG_DAEMON;
	config->log_config.min_level = LOG_INFO;
	config->log_config.options = LOG_PID | LOG_CONS;
	config->log_config.use_console = 0;
	config->log_config.ident = "tqftpserv";
}

/**
 * set_config_string() - Safely set a configuration string value
 * @dest: Destination buffer
 * @dest_size: Size of destination buffer
 * @src: Source string
 * @name: Configuration parameter name (for logging)
 */
static void set_config_string(char *dest, size_t dest_size, const char *src, const char *name)
{
	if (strlen(src) >= dest_size) {
		TQFTP_LOG_WARNING("Configuration value for '%s' too long, truncating", name);
	}
	strncpy(dest, src, dest_size - 1);
	dest[dest_size - 1] = '\0';
}

/**
 * tqftp_config_load_file() - Load configuration from file using libconfig
 * @config: Configuration structure to populate
 * @filename: Path to configuration file
 *
 * Uses libconfig library for robust configuration parsing with:
 * - Proper syntax validation
 * - Type checking
 * - Hierarchical configuration support
 * - Better error reporting
 *
 * Return: 0 on success, -1 on error
 */
int tqftp_config_load_file(struct tqftp_config *config, const char *filename)
{
	config_t cfg;
	const char *str_val;
	int int_val;
	int ret = 0;

	config_init(&cfg);

	/* Read the configuration file */
	if (!config_read_file(&cfg, filename)) {
		if (config_error_type(&cfg) != CONFIG_ERR_FILE_IO) {
			TQFTP_LOG_ERR("Configuration error in %s:%d - %s",
				      filename, config_error_line(&cfg), config_error_text(&cfg));
		} else {
			/* File doesn't exist - this is not an error for optional config */
			TQFTP_LOG_DEBUG("Configuration file %s not found, using defaults", filename);
		}
		config_destroy(&cfg);
		return -1;
	}

	TQFTP_LOG_INFO("Loading configuration from %s", filename);

	/* Load path configuration */
	if (config_lookup_string(&cfg, "readonly_path", &str_val)) {
		set_config_string(config->readonly_path, sizeof(config->readonly_path), str_val, "readonly_path");
		TQFTP_LOG_DEBUG("Config: readonly_path = %s", str_val);
	}

	if (config_lookup_string(&cfg, "readwrite_path", &str_val)) {
		set_config_string(config->readwrite_path, sizeof(config->readwrite_path), str_val, "readwrite_path");
		TQFTP_LOG_DEBUG("Config: readwrite_path = %s", str_val);
	}

	if (config_lookup_string(&cfg, "firmware_base", &str_val)) {
		set_config_string(config->firmware_base, sizeof(config->firmware_base), str_val, "firmware_base");
		TQFTP_LOG_DEBUG("Config: firmware_base = %s", str_val);
	}

	if (config_lookup_string(&cfg, "updates_dir", &str_val)) {
		set_config_string(config->updates_dir, sizeof(config->updates_dir), str_val, "updates_dir");
		TQFTP_LOG_DEBUG("Config: updates_dir = %s", str_val);
	}

	if (config_lookup_string(&cfg, "temp_dir", &str_val)) {
		set_config_string(config->temp_dir, sizeof(config->temp_dir), str_val, "temp_dir");
		TQFTP_LOG_DEBUG("Config: temp_dir = %s", str_val);
	}

	/* Load logging configuration */
	if (config_lookup_string(&cfg, "log_level", &str_val)) {
		int level = tqftp_parse_log_level(str_val);
		if (level >= 0) {
			config->log_config.min_level = level;
			TQFTP_LOG_DEBUG("Config: log_level = %s (%d)", str_val, level);
		} else {
			TQFTP_LOG_WARNING("Invalid log level '%s' in config file", str_val);
		}
	}

	/* Log facility is always LOG_DAEMON as per @lumag's feedback */
	if (config_lookup_string(&cfg, "log_facility", &str_val)) {
		config->log_config.facility = LOG_DAEMON;
		TQFTP_LOG_INFO("Log facility is fixed to 'daemon' (ignoring config value '%s')", str_val);
	}

	if (config_lookup_bool(&cfg, "log_console", &int_val)) {
		config->log_config.use_console = int_val ? 1 : 0;
		TQFTP_LOG_DEBUG("Config: log_console = %s", int_val ? "true" : "false");
	}

	/* Support both boolean and string formats for log_console */
	if (config_lookup_string(&cfg, "log_console", &str_val)) {
		if (strcasecmp(str_val, "yes") == 0 || strcasecmp(str_val, "true") == 0 || strcmp(str_val, "1") == 0) {
			config->log_config.use_console = 1;
		} else if (strcasecmp(str_val, "no") == 0 || strcasecmp(str_val, "false") == 0 || strcmp(str_val, "0") == 0) {
			config->log_config.use_console = 0;
		} else {
			TQFTP_LOG_WARNING("Invalid log_console value '%s' (use true/false or yes/no)", str_val);
		}
		TQFTP_LOG_DEBUG("Config: log_console = %s", config->log_config.use_console ? "true" : "false");
	}

	config_destroy(&cfg);
	TQFTP_LOG_INFO("Configuration loaded successfully from %s", filename);
	return ret;
}

/**
 * tqftp_config_print_help() - Print help message
 * @progname: Program name
 */
void tqftp_config_print_help(const char *progname)
{
	printf("Usage: %s [OPTIONS]\n", progname);
	printf("\nTQFTP Server - TFTP over QRTR\n");
	printf("\nPath Configuration:\n");
	printf("  --readonly-path PATH     Set readonly path prefix (default: %s)\n", DEFAULT_READONLY_PATH);
	printf("  --readwrite-path PATH    Set readwrite path prefix (default: %s)\n", DEFAULT_READWRITE_PATH);
	printf("  --firmware-base PATH     Set firmware base directory (default: %s)\n", DEFAULT_FIRMWARE_BASE);
	printf("  --updates-dir DIR        Set updates directory name (default: %s)\n", DEFAULT_UPDATES_DIR);
	printf("  --temp-dir PATH          Set temporary directory (default: %s)\n", DEFAULT_TEMP_DIR);
	printf("\nLogging Options:\n");
	printf("  -l, --log-level LEVEL    Set minimum log level\n");
	printf("                           (emerg, alert, crit, err, warn, notice, info, debug)\n");
	printf("                           Default: info\n");
	printf("  -c, --console            Log to console instead of syslog\n");
	printf("                           (Note: log facility is always 'daemon')\n");
	printf("\nConfiguration File:\n");
	printf("  --config FILE            Load configuration from file (default: %s)\n", DEFAULT_CONFIG_FILE);
	printf("  --no-config              Don't load default configuration file\n");
	printf("\nGeneral Options:\n");
	printf("  -h, --help               Show this help message\n");
	printf("\nConfiguration File Format (libconfig syntax):\n");
	printf("  # Comments start with # or //\n");
	printf("  readonly_path = \"/readonly/firmware/image/\";\n");
	printf("  readwrite_path = \"/readwrite/\";\n");
	printf("  firmware_base = \"/lib/firmware/\";\n");
	printf("  updates_dir = \"updates/\";\n");
	printf("  temp_dir = \"/tmp/tqftpserv\";\n");
	printf("  log_level = \"info\";\n");
	printf("  log_console = false;  # or true, yes, no\n");
	printf("\nExamples:\n");
	printf("  %s                                    # Run with default settings\n", progname);
	printf("  %s -l debug -c                       # Debug level logging to console\n", progname);
	printf("  %s --firmware-base /custom/firmware  # Use custom firmware directory\n", progname);
	printf("  %s --config /etc/custom.conf         # Use custom config file\n", progname);
	printf("\n");
}

/**
 * tqftp_config_parse_args() - Parse command line arguments
 * @argc: Argument count
 * @argv: Argument vector
 * @config: Configuration structure to populate
 *
 * Return: 0 on success, 1 if help shown, -1 on error
 */
int tqftp_config_parse_args(int argc, char **argv, struct tqftp_config *config)
{
	int opt;
	int level;
	int load_config_file = 1;

	static struct option long_options[] = {
		{"readonly-path", required_argument, 0, 1001},
		{"readwrite-path", required_argument, 0, 1002},
		{"firmware-base", required_argument, 0, 1003},
		{"updates-dir", required_argument, 0, 1004},
		{"temp-dir", required_argument, 0, 1005},
		{"config", required_argument, 0, 1006},
		{"no-config", no_argument, 0, 1007},
		{"log-level", required_argument, 0, 'l'},
		{"console", no_argument, 0, 'c'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "l:ch", long_options, NULL)) != -1) {
		switch (opt) {
		case 1001: /* --readonly-path */
			set_config_string(config->readonly_path, sizeof(config->readonly_path), optarg, "readonly-path");
			break;

		case 1002: /* --readwrite-path */
			set_config_string(config->readwrite_path, sizeof(config->readwrite_path), optarg, "readwrite-path");
			break;

		case 1003: /* --firmware-base */
			set_config_string(config->firmware_base, sizeof(config->firmware_base), optarg, "firmware-base");
			break;

		case 1004: /* --updates-dir */
			set_config_string(config->updates_dir, sizeof(config->updates_dir), optarg, "updates-dir");
			break;

		case 1005: /* --temp-dir */
			set_config_string(config->temp_dir, sizeof(config->temp_dir), optarg, "temp-dir");
			break;

		case 1006: /* --config */
			set_config_string(config->config_file, sizeof(config->config_file), optarg, "config");
			break;

		case 1007: /* --no-config */
			load_config_file = 0;
			break;

		case 'l':
			level = tqftp_parse_log_level(optarg);
			if (level < 0) {
				fprintf(stderr, "Invalid log level: %s\n", optarg);
				return -1;
			}
			config->log_config.min_level = level;
			break;

		case 'c':
			config->log_config.use_console = 1;
			break;

		case 'h':
			tqftp_config_print_help(argv[0]);
			return 1;

		default:
			tqftp_config_print_help(argv[0]);
			return -1;
		}
	}

	/* Load configuration file if requested */
	if (load_config_file && access(config->config_file, R_OK) == 0) {
		tqftp_config_load_file(config, config->config_file);
	}

	return 0;
}
