# Simple TCP Proxy Server

This project is a simple TCP proxy server capable of accepting incoming TCP connections, saving requests, and sending them to a PostgreSQL database for further processing.

## Requirements

- C++ compiler with C++17 support
- Berkeley sockets for working with PostgreSQL
- epoll library for handling multiplexing in Linux

## Installation and Build

1. Clone the repository:

    ```bash
    git clone https://github.com/burshlatt/TCP_proxy_server.git
    ```

2. Navigate to the project directory and build the server:

    ```bash
    cd PostgreSQL_TCP_proxy_server
    make build
    ```

## Uninstallation

1. To clean up the project:

    ```bash
    make clean
    ```

2. To delete the log files:

    ```bash
    make clean_log
    ```
    
3. To delete the doxygen files:

    ```bash
    make clean_docs
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
```

## Database preparation

Log into psql as the user "postgres":
```bash
psql -h 127.0.0.1 -U postgres
```
Create a new database "sbtest" and new user "sbtest":
```bash
CREATE USER sbtest WITH PASSWORD '12345';
CREATE DATABASE sbtest OWNER sbtest;
\c sbtest
ALTER SCHEMA public OWNER TO postgres;
GRANT USAGE, CREATE ON SCHEMA public TO sbtest;
```
After creating the database, run the command to fill the table with data:
```bash
make prepare_db
```

## Running

After successful compilation, the server can be started by executing the following command:

```bash
./server <port> # or 'make run' to start the server on port 5656
```

## Running tests

Run this command to run tests through sysbench:
```bash
make test
```

## Usage

1. Connect your client to the port on which the server is running.
2. Send queries to the server in a format consistent with the PostgreSQL network protocol (https://www.postgresql.org/docs/current/protocol-message-formats.html).
3. The server will save the received requests in the requests.log file and send them to the PostgreSQL database.
4. The server will send the response from the database to you.

## License

This project is licensed under the [MIT License](LICENSE).

---
