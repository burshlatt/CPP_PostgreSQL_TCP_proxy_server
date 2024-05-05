#!/bin/bash

DRIVER="pgsql"
PORT="4568"
HOST="127.0.0.1"
DB="sbtest"
USER="sbtest"
PASS="12345"
TEST_FILE="/usr/share/sysbench/oltp_read_write.lua"

TIME_SEC="300"
NUM_THREADS="1"
EVENTS="1000000"
TABLE_SIZE="100000"

sysbench \
    $TEST_FILE \
    --db-driver=$DRIVER \
    --pgsql-host=$HOST \
    --pgsql-port=$PORT \
    --pgsql-db=$DB \
	--pgsql-user=$USER \
	--pgsql-password=$PASS \
    --events=$EVENTS \
    --time=$TIME_SEC \
    --threads=$NUM_THREADS \
    --table-size=$TABLE_SIZE \
    run
