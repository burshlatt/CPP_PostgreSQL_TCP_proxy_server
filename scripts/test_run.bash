#!/bin/bash

DRIVER="pgsql"
PORT="5656"
HOST="127.0.0.1"
DB="sbtest"
USER="sbtest"
PASS="12345"

TEST_FILE="/usr/share/sysbench/oltp_insert.lua"
# TEST_FILE="/usr/share/sysbench/oltp_read_write.lua"
# TEST_FILE="/usr/share/sysbench/oltp_point_select.lua"
# TEST_FILE="/usr/share/sysbench/oltp_update_index.lua"
# TEST_FILE="/usr/share/sysbench/select_random_points.lua"

TIME_SEC="300"
NUM_THREADS="90"
NUM_TABLE="10"
TABLE_SIZE="10000"

sysbench $TEST_FILE \
    --db-driver=$DRIVER \
    --pgsql-host=$HOST \
    --pgsql-port=$PORT \
    --pgsql-db=$DB \
	--pgsql-user=$USER \
	--pgsql-password=$PASS \
    --db-ps-mode=disable \
    --time=$TIME_SEC \
    --threads=$NUM_THREADS \
    --tables=$NUM_TABLE \
    --table-size=$TABLE_SIZE \
    run
