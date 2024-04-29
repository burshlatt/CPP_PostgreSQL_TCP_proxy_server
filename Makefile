CXX = g++
FLAGS = -Wall -Werror -Wextra

install: clean
	$(CXX) $(FLAGS) src/main.cc src/server.cc -o server -lpqxx -lpq

setup_db:
	sysbench \
	/usr/share/sysbench/oltp_read_write.lua \
	--db-driver=pgsql \
	--table-size=100000 \
	--pgsql-host=127.0.0.1 \
	--pgsql-port=5432 \
	--pgsql-user=sbtest \
	--pgsql-password=12345 \
	--pgsql-db=sbtest \
	prepare

test:
	sysbench \
	/usr/share/sysbench/oltp_read_write.lua \
	--db-driver=pgsql \
	--table-size=100000 \
	--threads=1 \
	--pgsql-host=127.0.0.1 \
	--pgsql-port=5433 \
	--pgsql-user=sbtest \
	--pgsql-password=12345 \
	--pgsql-db=sbtest \
	run

clean:
	rm -rf server
