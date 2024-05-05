#!/bin/bash

DRIVER="pgsql"
PORT="4568"
HOST="127.0.0.1"
DB="sbtest"
USER="sbtest"
PASS="12345"
TEST_FILE="/usr/share/sysbench/oltp_write_only.lua"

TIME_SEC="10"
NUM_THREADS="1"
TABLE_SIZE="100000"

sysbench \
    $TEST_FILE \
    --db-driver=$DRIVER \
    --pgsql-host=$HOST \
    --pgsql-port=$PORT \
    --pgsql-db=$DB \
	--pgsql-user=$USER \
	--pgsql-password=$PASS \
    --time=$TIME_SEC \
    --threads=$NUM_THREADS \
    --table-size=$TABLE_SIZE \
    run
