#!/bin/sh
PATH=/bin:/usr/bin:/sbin:/usr/sbin

ALARMMGR_DUMP=$1/alarmmgr
mkdir -p $ALARMMGR_DUMP
cp -f /var/log/alarmmgr.log $ALARMMGR_DUMP

alarmmgr_get_all_info
mv -f /tmp/alarmmgr_* $ALARMMGR_DUMP
