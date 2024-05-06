CXX = g++
FLAGS = -Wall -Werror -Wextra

.PHONY: install run uninstall prepare_db test clean_log clean

install: uninstall
	$(CXX) $(FLAGS) src/*.cc -o server

run:
	./server 5656

uninstall:
	rm -rf server

prepare_db:
	sh scripts/prepare.bash

test:
	sh scripts/run.bash

clean_log:
	rm -rf requests.log

clean: clean_log uninstall
