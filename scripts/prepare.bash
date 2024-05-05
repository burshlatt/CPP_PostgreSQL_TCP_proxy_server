#!/bin/bash

DRIVER="pgsql"
PORT="5433"
HOST="127.0.0.1"
TEST_FILE="/usr/share/sysbench/oltp_read_write.lua"

DB="sbtest"
USER="sbtest"
PASS="12345"

TABLE_SIZE="100000"

sysbench \
	/usr/share/sysbench/oltp_read_write.lua \
	--db-driver=$DRIVER \
	--pgsql-host=$HOST \
	--pgsql-port=$PORT \
    --pgsql-db=$DB \
	--pgsql-user=$USER \
	--pgsql-password=$PASS \
	--table-size=$TABLE_SIZE \
	prepare