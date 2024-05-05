CXX = g++
FLAGS = -Wall -Werror -Wextra

install: clean
	$(CXX) $(FLAGS) src/main.cc src/server.cc -o server -lpqxx -lpq

prepare_db:
	sh scripts/prepare.bash

test:
	sh scripts/run.bash

clean:
	rm -rf server client requests.log
