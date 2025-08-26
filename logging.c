// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "logging.h"

/* Global logging configuration */
struct tqftp_log_config tqftp_log_cfg = {
	.facility = LOG_DAEMON,
	.min_level = LOG_INFO,
	.options = LOG_PID | LOG_CONS,
	.use_console = 0,
	.ident = "tqftpserv"
};

/* Log level name mappings */
static const struct {
	const char *name;
	int level;
} log_levels[] = {
	{"emerg", LOG_EMERG},
	{"emergency", LOG_EMERG},
	{"alert", LOG_ALERT},
	{"crit", LOG_CRIT},
	{"critical", LOG_CRIT},
	{"err", LOG_ERR},
	{"error", LOG_ERR},
	{"warn", LOG_WARNING},
	{"warning", LOG_WARNING},
	{"notice", LOG_NOTICE},
	{"info", LOG_INFO},
	{"debug", LOG_DEBUG},
	{NULL, -1}
};


/**
 * tqftp_log_init() - Initialize logging with default configuration
 * @ident: Program identifier for log messages
 */
void tqftp_log_init(const char *ident)
{
	tqftp_log_cfg.ident = ident;
	openlog(tqftp_log_cfg.ident, tqftp_log_cfg.options, tqftp_log_cfg.facility);
}

/**
 * tqftp_log_init_with_config() - Initialize logging with custom configuration
 * @config: Logging configuration structure
 */
void tqftp_log_init_with_config(struct tqftp_log_config *config)
{
	if (config) {
		tqftp_log_cfg = *config;
	}

	if (!tqftp_log_cfg.use_console) {
		openlog(tqftp_log_cfg.ident, tqftp_log_cfg.options, tqftp_log_cfg.facility);
	}
}

/**
 * tqftp_log() - Internal logging function
 * @priority: Syslog priority level
 * @fmt: Format string
 * @...: Format arguments
 */
void tqftp_log(int priority, const char *fmt, ...)
{
	va_list args;
	char buffer[1024];
	time_t now;
	struct tm *tm_info;
	char time_str[64];

	va_start(args, fmt);

	if (tqftp_log_cfg.use_console) {
		/* Log to console with timestamp */
		time(&now);
		tm_info = localtime(&now);
		strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

		vsnprintf(buffer, sizeof(buffer), fmt, args);
		fprintf(stderr, "%s [TQFTP] %s\n", time_str, buffer);
		fflush(stderr);
	} else {
		/* Log to syslog */
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		syslog(priority, "[TQFTP] %s", buffer);
	}

	va_end(args);
}

/**
 * tqftp_parse_log_level() - Parse log level string
 * @level_str: Log level string (e.g., "info", "debug", "error")
 *
 * Return: Log level constant on success, -1 on error
 */
int tqftp_parse_log_level(const char *level_str)
{
	int i;

	if (!level_str)
		return -1;

	for (i = 0; log_levels[i].name; i++) {
		if (!strcasecmp(level_str, log_levels[i].name)) {
			return log_levels[i].level;
		}
	}

	return -1;
}
