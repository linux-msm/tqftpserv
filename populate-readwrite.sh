#!/usr/bin/sh
# Populate tqftp readwrite data from persist partition

set -eu

PERSIST_DEV="/dev/disk/by-partlabel/persist"
PERSIST_MOUNT="/mnt/persist"
TQFTPSERV_TMP="/var/lib/tqftpserv"

if [ -f $TQFTPSERV_TMP/readwrite_ready ]; then
	exit 0
fi

mkdir -p $PERSIST_MOUNT
mount $PERSIST_DEV -o ro,noatime $PERSIST_MOUNT
mkdir -p $TQFTPSERV_TMP
cp -r $PERSIST_MOUNT/rfs/msm/mpss/* $TQFTPSERV_TMP/
touch $TQFTPSERV_TMP/readwrite_ready
umount $PERSIST_MOUNT
rmdir $PERSIST_MOUNT
