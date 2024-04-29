# Simple TCP Proxy Server

This project is a simple TCP proxy server capable of accepting incoming TCP connections, saving requests, and sending them to a PostgreSQL database for further processing.

## Requirements

- C++ compiler with C++11 support
- libpqxx library for working with PostgreSQL
- epoll library for handling multiplexing in Linux

## Installation and Build

1. Install the required libraries and compiler if not already installed:

    ```bash
    sudo apt-get update
    sudo apt-get install build-essential libpqxx-dev
    ```

2. Clone the repository:

    ```bash
    git clone https://github.com/burshlatt/TCP_proxy_server.git
    ```

3. Navigate to the project directory and build the server:

    ```bash
    cd TCP_proxy_server
    make install
    ```

## Installing sysbench

Install sysbench using these commands:
```bash
curl -s https://packagecloud.io/install/repositories/akopytov/sysbench/script.deb.sh | sudo bash
sudo apt -y install sysbench
```
Verify the version:
```bash
sysbench --version
sysbench 1.0.20
```

## Database preparation

Create a new database "sbtest" and new user "sbtest":

```bash
psql -h 127.0.0.1 -U postgres
```
After connecting to psql:
```bash
CREATE USER sbtest WITH PASSWORD '12345';
CREATE DATABASE sbtest;
GRANT ALL PRIVILEGES ON DATABASE sbtest TO sbtest;
```
After creating the database, run the command to fill the table with data:
```bash
make setup_bd
```

## Running

After successful compilation, the server can be started by executing the following command:

```bash
./server <port>
```

## Running tests

Run this command to run tests through sysbench:
```bash
make test
```

## Usage

1. Connect your client to the port where the server is running.
2. Send requests to the server.
3. The server will save the received requests in the `requests.log` file and send them to the PostgreSQL database.

## License

This project is licensed under the [MIT License](LICENSE).

---
