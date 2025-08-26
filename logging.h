// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __LOGGING_H__
#define __LOGGING_H__

#include <syslog.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

/* Logging configuration structure */
struct tqftp_log_config {
	int facility;
	int min_level;
	int options;
	int use_console;
	const char *ident;
};

/* Global logging configuration */
extern struct tqftp_log_config tqftp_log_cfg;

/* Initialize syslog for the application */
#define LOG_INIT(ident) tqftp_log_init(ident)
#define LOG_INIT_WITH_CONFIG(config) tqftp_log_init_with_config(config)

/* Close syslog */
#define LOG_CLOSE() closelog()

/* Function prototypes */
void tqftp_log_init(const char *ident);
void tqftp_log_init_with_config(struct tqftp_log_config *config);
int tqftp_parse_log_level(const char *level_str);

/* Internal logging function */
void tqftp_log(int priority, const char *fmt, ...);

/* Project-specific logging macros with different priority levels */
#define TQFTP_LOG_EMERG(fmt, ...)   do { if (LOG_EMERG >= tqftp_log_cfg.min_level) tqftp_log(LOG_EMERG, fmt, ##__VA_ARGS__); } while(0)
#define TQFTP_LOG_ALERT(fmt, ...)   do { if (LOG_ALERT >= tqftp_log_cfg.min_level) tqftp_log(LOG_ALERT, fmt, ##__VA_ARGS__); } while(0)
#define TQFTP_LOG_CRIT(fmt, ...)    do { if (LOG_CRIT >= tqftp_log_cfg.min_level) tqftp_log(LOG_CRIT, fmt, ##__VA_ARGS__); } while(0)
#define TQFTP_LOG_ERR(fmt, ...)     do { if (LOG_ERR >= tqftp_log_cfg.min_level) tqftp_log(LOG_ERR, fmt, ##__VA_ARGS__); } while(0)
#define TQFTP_LOG_WARNING(fmt, ...) do { if (LOG_WARNING >= tqftp_log_cfg.min_level) tqftp_log(LOG_WARNING, fmt, ##__VA_ARGS__); } while(0)
#define TQFTP_LOG_NOTICE(fmt, ...)  do { if (LOG_NOTICE >= tqftp_log_cfg.min_level) tqftp_log(LOG_NOTICE, fmt, ##__VA_ARGS__); } while(0)
#define TQFTP_LOG_INFO(fmt, ...)    do { if (LOG_INFO >= tqftp_log_cfg.min_level) tqftp_log(LOG_INFO, fmt, ##__VA_ARGS__); } while(0)
#define TQFTP_LOG_DEBUG(fmt, ...)   do { if (LOG_DEBUG >= tqftp_log_cfg.min_level) tqftp_log(LOG_DEBUG, fmt, ##__VA_ARGS__); } while(0)

#endif /* __LOGGING_H__ */
