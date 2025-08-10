// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Linaro Ltd.
 */
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <libqrtr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "list.h"
#include "translate.h"
#include "logging.h"
#include "config.h"

#define MAX(x, y) ((x) > (y) ? (x) : (y))

enum {
	OP_RRQ = 1,
	OP_WRQ,
	OP_DATA,
	OP_ACK,
	OP_ERROR,
	OP_OACK,
};

enum {
	ERROR_END_OF_TRANSFER = 9,
};

struct tftp_client {
	struct list_head node;

	struct sockaddr_qrtr sq;

	int sock;
	int fd;

	size_t block;

	size_t blksize;
	size_t rsize;
	size_t wsize;
	unsigned int timeoutms;
	off_t seek;

	uint8_t *blk_buf;
	uint8_t *rw_buf;
	size_t rw_buf_size;
	size_t blk_offset;
	uint16_t blk_expected;
};

static struct list_head readers = LIST_INIT(readers);
static struct list_head writers = LIST_INIT(writers);

static ssize_t tftp_send_data(struct tftp_client *client,
			      unsigned int block, size_t offset, size_t response_size)
{
	ssize_t len;
	ssize_t send_len;
	uint8_t *buf = client->blk_buf;
	uint8_t *p = buf;

	*p++ = 0;
	*p++ = OP_DATA;

	*p++ = (block >> 8) & 0xff;
	*p++ = block & 0xff;

	len = pread(client->fd, p, client->blksize, offset);
	if (len < 0) {
		TQFTP_LOG_ERR("failed to read data");
		free(buf);
		return len;
	}

	p += len;

	/* If rsize was set, we should limit the data in the response to n bytes */
	if (response_size != 0) {
		/* Header (4 bytes) + data size */
		send_len = 4 + response_size;
		if (send_len > p - buf) {
			printf("[TQFTP] requested data of %ld bytes but only read %ld bytes from file, rejecting\n", response_size, len);
			free(buf);
			return -EINVAL;
		}
	} else {
		send_len = p - buf;
	}

	// printf("[TQFTP] Sending %zd bytes of DATA\n", send_len);
	return send(client->sock, buf, send_len, 0);
}


static int tftp_send_ack(int sock, int block)
{
	struct {
		uint16_t opcode;
		uint16_t block;
	} ack = { htons(OP_ACK), htons(block) };

	return send(sock, &ack, sizeof(ack), 0);
}

static int tftp_send_oack(int sock, size_t *blocksize, ssize_t *tsize,
			  size_t *wsize, unsigned int *timeoutms, size_t *rsize,
			  off_t *seek)
{
	char buf[512];
	char *p = buf;
	int n;

	*p++ = 0;
	*p++ = OP_OACK;

	if (blocksize) {
		strcpy(p, "blksize");
		p += 8;

		n = sprintf(p, "%zd", *blocksize);
		p += n;
		*p++ = '\0';
	}

	if (timeoutms) {
		strcpy(p, "timeoutms");
		p += 10;

		n = sprintf(p, "%d", *timeoutms);
		p += n;
		*p++ = '\0';
	}

	if (tsize && *tsize != -1) {
		strcpy(p, "tsize");
		p += 6;

		n = sprintf(p, "%zd", *tsize);
		p += n;
		*p++ = '\0';
	}

	if (wsize) {
		strcpy(p, "wsize");
		p += 6;

		n = sprintf(p, "%zd", *wsize);
		p += n;
		*p++ = '\0';
	}

	if (rsize) {
		strcpy(p, "rsize");
		p += 6;

		n = sprintf(p, "%zd", *rsize);
		p += n;
		*p++ = '\0';
	}

	if (seek) {
		strcpy(p, "seek");
		p += 5;

		n = sprintf(p, "%zd", *seek);
		p += n;
		*p++ = '\0';
	}

	return send(sock, buf, p - buf, 0);
}

static int tftp_send_error(int sock, int code, const char *msg)
{
	size_t len;
	char *buf;
	int rc;

	len = 4 + strlen(msg) + 1;

	buf = calloc(1, len);
	if (!buf)
		return -1;

	*(uint16_t*)buf = htons(OP_ERROR);
	*(uint16_t*)(buf + 2) = htons(code);
	strcpy(buf + 4, msg);

	rc = send(sock, buf, len, 0);
	free(buf);
	return rc;
}

static void parse_options(const char *buf, size_t len, size_t *blksize,
			  ssize_t *tsize, size_t *wsize, unsigned int *timeoutms,
			  size_t *rsize, off_t *seek)
{
	const char *opt, *value;
	const char *p = buf;

	while (p < buf + len) {
		/* XXX: ensure we're not running off the end */
		opt = p;
		p += strlen(p) + 1;

		/* XXX: ensure we're not running off the end */
		value = p;
		p += strlen(p) + 1;

		/*
		 * blksize: block size - how many bytes to send at once
		 * timeoutms: timeout in milliseconds
		 * tsize: total size - request to get file size in bytes
		 * rsize: read size - how many bytes to send, not full file
		 * wsize: window size - how many blocks to send without ACK
		 * seek: offset from beginning of file in bytes to start reading
		 */
		if (!strcmp(opt, "blksize")) {
			*blksize = atoi(value);
		} else if (!strcmp(opt, "timeoutms")) {
			*timeoutms = atoi(value);
		} else if (!strcmp(opt, "tsize")) {
			*tsize = atoi(value);
		} else if (!strcmp(opt, "rsize")) {
			*rsize = atoi(value);
		} else if (!strcmp(opt, "wsize")) {
			*wsize = atoi(value);
		} else if (!strcmp(opt, "seek")) {
			*seek = atoi(value);
		} else {
			TQFTP_LOG_WARN("Ignoring unknown option '%s' with value '%s'", opt, value);
		}
	}
}

static void handle_rrq(const char *buf, size_t len, struct sockaddr_qrtr *sq)
{
	struct tftp_client *client;
	const char *filename;
	const char *mode;
	struct stat sb;
	const char *p;
	ssize_t tsize = -1;
	size_t blksize = 512;
	unsigned int timeoutms = 1000;
	size_t rsize = 0;
	size_t wsize = 1;
	off_t seek = 0;
	bool do_oack = false;
	int sock;
	int ret;
	int fd;

	p = buf + 2;

	filename = p;
	p += strlen(p) + 1;

	mode = p;
	p += strlen(p) + 1;

	if (strcasecmp(mode, "octet")) {
		/* XXX: error */
		TQFTP_LOG_ERR("Mode is not octet, reject");
		return;
	}

	if (p < buf + len) {
		do_oack = true;
		parse_options(p, len - (p - buf), &blksize, &tsize, &wsize,
				&timeoutms, &rsize, &seek);
	}

	TQFTP_LOG_DEBUG("RRQ: %s (mode=%s rsize=%ld seek=%ld)", filename, mode, rsize, seek);

	sock = qrtr_open(0);
	if (sock < 0) {
		/* XXX: error */
		TQFTP_LOG_ERR("unable to create new qrtr socket, reject");
		return;
	}

	ret = connect(sock, (struct sockaddr *)sq, sizeof(*sq));
	if (ret < 0) {
		/* XXX: error */
		TQFTP_LOG_ERR("unable to connect new qrtr socket to remote");
		return;
	}

	fd = translate_open(filename, O_RDONLY);
	if (fd < 0) {
		TQFTP_LOG_ERR("unable to open %s (%d), reject", filename, errno);
		tftp_send_error(sock, 1, "file not found");
		return;
	}

	if (tsize != -1) {
		fstat(fd, &sb);
		tsize = sb.st_size;
	}

	client = calloc(1, sizeof(*client));
	client->sq = *sq;
	client->sock = sock;
	client->fd = fd;
	client->blksize = blksize;
	client->rsize = rsize;
	client->wsize = wsize;
	client->timeoutms = timeoutms;
	client->seek = seek;
	client->rw_buf_size = blksize * wsize;

	client->blk_buf = calloc(1, blksize + 4);
	if (!client->blk_buf) {
		TQFTP_LOG_ERR("Memory allocation failure");
		return;
	}

	client->rw_buf = calloc(1, client->rw_buf_size);
	if (!client->rw_buf) {
		TQFTP_LOG_ERR("Memory allocation failure");
		return;
	}

	TQFTP_LOG_DEBUG("new reader added");

	list_add(&readers, &client->node);

	if (do_oack) {
		tftp_send_oack(client->sock, &blksize,
			       tsize ? &tsize : NULL,
			       wsize ? &wsize : NULL,
			       &client->timeoutms,
			       rsize ? &rsize : NULL,
			       seek ? &seek : NULL);
	} else {
		tftp_send_data(client, 1, 0, 0);
	}
}

static void handle_wrq(const char *buf, size_t len, struct sockaddr_qrtr *sq)
{
	struct tftp_client *client;
	const char *filename;
	const char *mode;
	const char *p;
	ssize_t tsize = -1;
	size_t blksize = 512;
	unsigned int timeoutms = 1000;
	size_t rsize = 0;
	size_t wsize = 1;
	off_t seek = 0;
	bool do_oack = false;
	int sock;
	int ret;
	int fd;

	filename = buf + 2;
	mode = buf + 2 + strlen(filename) + 1;
	p = mode + strlen(mode) + 1;

	if (strcasecmp(mode, "octet")) {
		/* XXX: error */
		TQFTP_LOG_ERR("not octet, reject");
		return;
	}

	TQFTP_LOG_DEBUG("WRQ: %s (%s)", filename, mode);

	if (p < buf + len) {
		do_oack = true;
		parse_options(p, len - (p - buf), &blksize, &tsize, &wsize,
				&timeoutms, &rsize, &seek);
	}

	fd = translate_open(filename, O_WRONLY | O_CREAT);
	if (fd < 0) {
		/* XXX: error */
		TQFTP_LOG_ERR("unable to open %s (%d), reject", filename, errno);
		return;
	}

	sock = qrtr_open(0);
	if (sock < 0) {
		/* XXX: error */
		TQFTP_LOG_ERR("unable to create new qrtr socket, reject");
		return;
	}

	ret = connect(sock, (struct sockaddr *)sq, sizeof(*sq));
	if (ret < 0) {
		/* XXX: error */
		TQFTP_LOG_ERR("unable to connect new qrtr socket to remote");
		return;
	}

	client = calloc(1, sizeof(*client));
	client->sq = *sq;
	client->sock = sock;
	client->fd = fd;
	client->blksize = blksize;
	client->rsize = rsize;
	client->wsize = wsize;
	client->timeoutms = timeoutms;
	client->seek = seek;
	client->rw_buf_size = blksize * wsize;
	client->blk_expected = 1;

	client->blk_buf = calloc(1, blksize + 4);
	if (!client->blk_buf) {
		TQFTP_LOG_ERR("Memory allocation failure");
		return;
	}

	client->rw_buf = calloc(1, client->rw_buf_size);
	if (!client->rw_buf) {
		TQFTP_LOG_ERR("Memory allocation failure");
		return;
	}

	TQFTP_LOG_DEBUG("new writer added");

	list_add(&writers, &client->node);

	if (do_oack) {
		tftp_send_oack(client->sock, &blksize,
			       tsize ? &tsize : NULL,
			       wsize ? &wsize : NULL,
			       &client->timeoutms,
			       rsize ? &rsize : NULL,
			       seek ? &seek : NULL);
	} else {
		tftp_send_data(client, 1, 0, 0);
	}
}

static int handle_reader(struct tftp_client *client)
{
	struct sockaddr_qrtr sq;
	uint16_t block;
	uint16_t last;
	char buf[128];
	socklen_t sl;
	ssize_t len;
	ssize_t n = 0;
	int opcode;
	int ret;

	sl = sizeof(sq);
	len = recvfrom(client->sock, buf, sizeof(buf), 0, (void *)&sq, &sl);
	if (len < 0) {
		ret = -errno;
		if (ret != -ENETRESET)
			TQFTP_LOG_ERROR("recvfrom failed: %d", ret);
		return -1;
	}

	/* Drop unsolicited messages */
	if (sq.sq_node != client->sq.sq_node ||
	    sq.sq_port != client->sq.sq_port) {
		TQFTP_LOG_ERR("Discarding spoofed message");
		return -1;
	}

	opcode = buf[0] << 8 | buf[1];
	if (opcode == OP_ERROR) {
		buf[len] = '\0';
		int err = buf[2] << 8 | buf[3];
		/* "End of Transfer" is not an error, used with stat(2)-like calls */
		if (err == ERROR_END_OF_TRANSFER)
			TQFTP_LOG_DEBUG("Remote returned END OF TRANSFER: %d - %s", err, buf + 4);
		else
			TQFTP_LOG_ERR("Remote returned an error: %d - %s", err, buf + 4);
		return -1;
	} else if (opcode != OP_ACK) {
		TQFTP_LOG_ERR("Expected ACK, got %d", opcode);
		return -1;
	}

	last = buf[2] << 8 | buf[3];
	// printf("[TQFTP] Got ack for %d\n", last);

	/* We've sent enough data for rsize already */
	if (last * client->blksize > client->rsize)
		return 0;

	for (block = last; block < last + client->wsize; block++) {
		size_t offset = client->seek + block * client->blksize;
		size_t response_size = 0;
		/* Check if need to limit response size based for requested rsize */
		if ((block + 1) * client->blksize > client->rsize)
			response_size = client->rsize % client->blksize;

		n = tftp_send_data(client, block + 1,
				   offset, response_size);
		if (n < 0) {
			TQFTP_LOG_ERR("Sent block %d failed: %zd", block + 1, n);
			break;
		}
		// printf("[TQFTP] Sent block %d of %zd\n", block + 1, n);
		if (n == 0)
			break;
		/* We've sent enough data for rsize already */
		if ((block + 1) * client->blksize > client->rsize)
			break;
	}

	return 1;
}

static int handle_writer(struct tftp_client *client)
{
	struct sockaddr_qrtr sq;
	uint16_t block;
	size_t payload;
	uint8_t *buf = client->blk_buf;
	socklen_t sl;
	ssize_t len;
	int opcode;
	int ret;

	sl = sizeof(sq);
	len = recvfrom(client->sock, buf, client->blksize + 4, 0, (void *)&sq, &sl);
	if (len < 0) {
		ret = -errno;
		if (ret != -ENETRESET)
			TQFTP_LOG_ERROR("recvfrom failed: %d", ret);
		return -1;
	}

	/* Drop unsolicited messages */
	if (sq.sq_node != client->sq.sq_node ||
	    sq.sq_port != client->sq.sq_port)
		return -1;

	opcode = buf[0] << 8 | buf[1];
	block = buf[2] << 8 | buf[3];
	if (opcode != OP_DATA) {
		TQFTP_LOG_ERR("Expected DATA opcode, got %d", opcode);
		tftp_send_error(client->sock, 4, "Expected DATA opcode");
		return -1;
	}

	payload = len - 4;
	buf += 4;

	/* Check if we recieved expected block */
	if (block != client->blk_expected) {
		uint16_t blk_expected = client->blk_expected;

		TQFTP_LOG_ERR("Block number out of sequence: %d (expected %d)",
			 block, blk_expected);
		tftp_send_error(client->sock, 4, "Block number out of sequence");

		/* Set blk_expected to beginning of current window */
		if ((blk_expected % client->wsize) == 0)
			blk_expected -= client->wsize + 1;
		else
			blk_expected -= (blk_expected % client->wsize) - 1;

		client->blk_expected = blk_expected;
		client->blk_offset = 0;

		return -1;
	}

	client->blk_expected++;

	/* Copy the data to the destination buffer */
	memcpy(client->rw_buf + client->blk_offset, buf, payload);
	client->blk_offset += payload;

	/* Write to file if all the wsize blocks are recieved */
	if (block % client->wsize == 0) {
		ret = write(client->fd, client->rw_buf, client->blk_offset);
		if (ret < 0) {
			/* XXX: report error */
			TQFTP_LOG_ERR("failed to write data");
			return -1;
		}

		client->blk_offset = 0;
		tftp_send_ack(client->sock, block);
	}

	return payload == client->blksize ? 1 : 0;
}

static void client_close_and_free(struct tftp_client *client)
{
	list_del(&client->node);
	close(client->sock);
	close(client->fd);
	free (client->blk_buf);
	free (client->rw_buf);
	free(client);
}

int main(int argc, char **argv)
{
	struct tftp_client *client;
	struct tftp_client *next;
	struct sockaddr_qrtr sq;
	struct qrtr_packet pkt;
	socklen_t sl;
	ssize_t len;
	char buf[4096];
	fd_set rfds;
	int nfds;
	int opcode;
	int ret;
	int fd;

	/* Initialize configuration with defaults */
	tqftp_config_init_defaults(&tqftp_config);

	/* Parse command line arguments */
	ret = tqftp_config_parse_args(argc, argv, &tqftp_config);
	if (ret == 1) {
		/* Help was shown */
		return 0;
	} else if (ret < 0) {
		/* Error in arguments */
		return 1;
	}

	/* Initialize logging */
	tqftp_log_init_with_config(&tqftp_config.log_config);

	TQFTP_LOG_INFO("TQFTP server starting");
	TQFTP_LOG_DEBUG("Configuration: readonly_path=%s, readwrite_path=%s, firmware_base=%s, temp_dir=%s",
			tqftp_config.readonly_path, tqftp_config.readwrite_path,
			tqftp_config.firmware_base, tqftp_config.temp_dir);

	fd = qrtr_open(0);
	if (fd < 0) {
		TQFTP_LOG_ERR("failed to open qrtr socket");
		exit(1);
	}

	ret = qrtr_publish(fd, 4096, 1, 0);
	if (ret < 0) {
		TQFTP_LOG_ERR("failed to publish service registry service");
		exit(1);
	}

	TQFTP_LOG_INFO("TQFTP server ready, listening for connections");

	for (;;) {
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		nfds = fd;

		list_for_each_entry(client, &writers, node) {
			FD_SET(client->sock, &rfds);
			nfds = MAX(nfds, client->sock);
		}

		list_for_each_entry(client, &readers, node) {
			FD_SET(client->sock, &rfds);
			nfds = MAX(nfds, client->sock);
		}

		ret = select(nfds + 1, &rfds, NULL, NULL, NULL);
		if (ret < 0) {
			if (errno == EINTR) {
				continue;
			} else {
				TQFTP_LOG_ERROR("select failed");
				break;
			}
		}

		list_for_each_entry_safe(client, next, &writers, node) {
			if (FD_ISSET(client->sock, &rfds)) {
				ret = handle_writer(client);
				if (ret <= 0)
					client_close_and_free(client);
			}
		}

		list_for_each_entry_safe(client, next, &readers, node) {
			if (FD_ISSET(client->sock, &rfds)) {
				ret = handle_reader(client);
				if (ret <= 0)
					client_close_and_free(client);
			}
		}

		if (FD_ISSET(fd, &rfds)) {
			sl = sizeof(sq);
			len = recvfrom(fd, buf, sizeof(buf), 0, (void *)&sq, &sl);
			if (len < 0) {
				ret = -errno;
				if (ret != -ENETRESET)
					TQFTP_LOG_ERROR("recvfrom failed: %d", ret);
				return ret;
			}

			/* Ignore control messages */
			if (sq.sq_port == QRTR_PORT_CTRL) {
				ret = qrtr_decode(&pkt, buf, len, &sq);
				if (ret < 0) {
					TQFTP_LOG_ERROR("unable to decode qrtr packet");
					return ret;
				}

				switch (pkt.type) {
				case QRTR_TYPE_BYE:
					TQFTP_LOG_DEBUG("Got bye on QRTE_PORT_CTRL port");
					list_for_each_entry_safe(client, next, &writers, node) {
						if (client->sq.sq_node == sq.sq_node)
							client_close_and_free(client);
					}
					break;
				case QRTR_TYPE_DEL_CLIENT:
					TQFTP_LOG_DEBUG("Got DEL_CLIENT on QRTE_PORT_CTRL port");
					list_for_each_entry_safe(client, next, &writers, node) {
						if (!memcmp(&client->sq, &sq, sizeof(sq)))
							client_close_and_free(client);
					}
					break;
				}
			} else {
				if (len < 2)
					continue;

				opcode = buf[0] << 8 | buf[1];
				switch (opcode) {
				case OP_RRQ:
					TQFTP_LOG_DEBUG("Got OP_WRQ");
					handle_rrq(buf, len, &sq);
					break;
				case OP_WRQ:
					TQFTP_LOG_DEBUG("Got OP_WRQ");
					handle_wrq(buf, len, &sq);
					break;
				case OP_ERROR:
					buf[len] = '\0';
					TQFTP_LOG_ERR("received error: %d - %s", buf[2] << 8 | buf[3], buf + 4);
					break;
				default:
					TQFTP_LOG_ERR("unhandled op %d", opcode);
					break;
				}
			}
		}
	}

	close(fd);

	return 0;
}
