CXX = g++
FLAGS = -Wall -Werror -Wextra

FILES = \
	src/main.cc \
	src/server/server.cc \
	src/server/unique_fd/unique_fd.cc \
	src/server/sql_logger/sql_logger.cc

.PHONY: build run prepare_db test clean_db clean_log clean_docs clean

build:
	$(CXX) $(FLAGS) $(FILES) -o server

run:
	./server 5656

prepare_db:
	sh scripts/prepare.bash

test:
	sh scripts/test_run.bash

docs:
	doxygen Doxyfile

clean_db:
	sh scripts/cleanup.bash

clean_log:
	rm -rf requests.log

clean_docs:
	rm -rf docs

clean: clean_log clean_docs
	rm -rf server
