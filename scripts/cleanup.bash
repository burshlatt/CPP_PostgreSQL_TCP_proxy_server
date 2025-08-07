#!/bin/bash

DRIVER="pgsql"
PORT="5432"
HOST="127.0.0.1"

DB="sbtest"
USER="sbtest"
PASS="12345"

NUM_TABLE="10"
TABLE_SIZE="10000"

sysbench /usr/share/sysbench/oltp_read_write.lua \
	--db-driver=$DRIVER \
	--pgsql-host=$HOST \
	--pgsql-port=$PORT \
    --pgsql-db=$DB \
	--pgsql-user=$USER \
	--pgsql-password=$PASS \
	--tables=$NUM_TABLE \
	--table-size=$TABLE_SIZE \
	cleanup
