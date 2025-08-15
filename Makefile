CXX = g++
FLAGS = -Wall -Werror -Wextra -pthread -std=c++17

FILES = \
	src/main.cc \
	src/server/server.cc \
	src/server/logger/logger.cc \
	src/server/session/session.cc \
	src/server/unique_fd/unique_fd.cc

.PHONY: build run prepare_db test clean_db clean_log clean_docs clean

build:
	$(CXX) $(FLAGS) $(FILES) -o server

run:
	./server 5656 127.0.0.1 5432 requests.log

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
