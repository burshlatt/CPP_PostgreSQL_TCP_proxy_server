CXX = g++
FLAGS = -Wall -Werror -Wextra

.PHONY: build run prepare_db test clean_log clean

build:
	$(CXX) $(FLAGS) src/*.cc -o server

run:
	./server 5656

prepare_db:
	sh scripts/prepare.bash

clean_db:
	sh scripts/cleanup.bash

test:
	sh scripts/test_run.bash

clean_log:
	rm -rf requests.log

clean: clean_log
	rm -rf server
