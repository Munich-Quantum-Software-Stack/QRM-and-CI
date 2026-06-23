# RabbitMQ Docker Setup Guide

## Overview

This guide covers how to set up RabbitMQ in its own Docker container and connect a development container to it for use with the MQSS messaging stack.

---

## Prerequisites

- Docker installed and running
- Docker Compose (optional)

---

## 1. Setting Up the RabbitMQ Container

### Option A (Tested) — Docker CLI

```bash
docker run -d \
  --name rabbitmq \
  --hostname rabbitmq \
  -p 5672:5672 \
  -p 15672:15672 \
  rabbitmq:3-management
```

| Port  | Purpose                        |
|-------|--------------------------------|
| 5672  | AMQP protocol (messaging)      |
| 15672 | Management UI (HTTP)           |

> **Important:** Always use port `5672` for your application connections. Port `15672` is HTTP only — connecting your AMQP client to it will cause an immediate "connection closed unexpectedly" error.

### Option B — Docker Compose

Create a `docker-compose.yml` at the root of your project:

```yaml
services:
  rabbitmq:
    image: rabbitmq:3-management
    container_name: rabbitmq
    hostname: rabbitmq
    ports:
      - "5672:5672"
      - "15672:15672"
    environment:
      RABBITMQ_DEFAULT_USER: myuser
      RABBITMQ_DEFAULT_PASS: mypassword
      RABBITMQ_LOOPBACK_USERS: none
    networks:
      - mqss-net

  dev:
    build: .
    container_name: mqss-dev
    depends_on:
      - rabbitmq
    networks:
      - mqss-net

networks:
  mqss-net:
    driver: bridge
```

Start both containers:

```bash
docker compose up -d
```

## 2. Connecting the Dev Container to RabbitMQ

### Networking

Both containers must be on the **same Docker network**. If using Docker Compose, this is handled automatically via the shared `mqss-net` network. If using Docker CLI, run the commands:

```bash
# Create a shared network
docker network create mqss-net

# Connect both containers to it
docker network connect mqss-net rabbitmq
docker network connect mqss-net mqss-dev
```

Once on the same network, use the **container name** as the hostname:

```cpp
mqss::TransportOptions<mqss::RabbitMqSimple> opts;
opts.host     = "rabbitmq";    // container name; use "localhost" for local installs
opts.port     = 5672;
opts.username = "myuser";
opts.password = "mypassword";
```

---

## 3. Creating a Dedicated RabbitMQ User

By default, RabbitMQ restricts the `guest` user to `localhost` connections only. Since your dev container connects from a different container, create a dedicated user:

```bash
docker exec -it rabbitmq bash

rabbitmqctl add_user myuser mypassword
rabbitmqctl set_user_tags myuser administrator
rabbitmqctl set_permissions -p / myuser ".*" ".*" ".*"
```

Alternatively, pass credentials via environment variables in `docker-compose.yml` (see Option B above):

```yaml
environment:
  RABBITMQ_DEFAULT_USER: myuser
  RABBITMQ_DEFAULT_PASS: mypassword
  RABBITMQ_LOOPBACK_USERS: none
```

---

### Passing Connection Details (Currently within header files)

The important AMQP variables have been defined in:</br>

```c++
QRM/include/ConnectionHandler.hpp
```

You can modify these as per your requirements.</br>
Then in your C++ code:

```cpp
mqss::TransportOptions<mqss::RabbitMqSimple> opts;
opts.host     = AMQP_SERVER;    // "rabbitmq" (container name); use "localhost" for local installs
opts.port     = AMQP_PORT;      // 5672
opts.username = AMQP_USER;      // "myuser"
opts.password = AMQP_PASSWORD;  // "mypassword"
opts.vhost    = AMQP_VHOST;     // "/"
```

---

## 4. Verifying the Connection

### Check the Management UI

Open `http://localhost:15672` in your browser and log in with your credentials. You should see:

- **Connections** tab: active connections from your dev container
- **Queues** tab: queues created by your executables

### Check from the Terminal

```bash
# Verify the AMQP port is reachable from the dev container
nc -zv rabbitmq 5672

# List active connections
docker exec -it rabbitmq rabbitmqctl list_connections

# List queues
docker exec -it rabbitmq rabbitmqadmin list queues
```

---

## 5. Avoiding Common Pitfalls

### Stale Consumers

If you kill your executables without clean shutdown, RabbitMQ may retain stale consumers on queues. This can cause multiple consumers competing for the same messages. To clear stale state:

```bash
# Close all connections
docker exec -it rabbitmq rabbitmqctl close_all_connections "clearing stale connections"

# Delete queues (they will be recreated on next run)
docker exec -it rabbitmq rabbitmqadmin delete queue name=compiler.tasks.queue
docker exec -it rabbitmq rabbitmqadmin delete queue name=scheduler.tasks.queue
docker exec -it rabbitmq rabbitmqadmin delete queue name=test.results.queue
docker exec -it rabbitmq rabbitmqadmin delete queue name=submitter.tasks.queue
```

Or simply restart the container to reset everything:

```bash
docker restart rabbitmq
```

### Isolating Dev Queues

If multiple MQSS deployments share the same broker, prefix your queue names to avoid cross-contamination:

```cpp
const std::string compiler_queue = "compiler.tasks.queue";
const std::string scheduler_queue = "scheduler.tasks.queue";
const std::string results_queue = "test.results.queue";
const std::string submitter_queue = "submitter.tasks.queue";
```

Note: Currently these queues are set within the root CMakeLists.txt i.e. ```QRM/CMakeLists.txt```.

### Clean Shutdown of Executables

Ensure executables close their RabbitMQ connections cleanly on exit by handling signals:

```cpp
std::signal(SIGINT, [](int) {
    Logger::cleanup();
    exit(0);
});

std::signal(SIGTERM, [](int) {
    Logger::cleanup();
    exit(0);
});
```

And in your `start.sh` launch script, use `SIGTERM` with `wait` to allow clean shutdown:

```bash
trap "kill -SIGTERM $SCHEDULER_PID $COMPILER_PID $TEST_PID; wait; echo 'Stopped.'" SIGINT SIGTERM
```

---

## 6. Quick Reference

| Task                          | Command                                                                 |
|-------------------------------|-------------------------------------------------------------------------|
| Start RabbitMQ container      | `docker compose up -d`                                                  |
| Stop RabbitMQ container       | `docker compose down`                                                   |
| Open management UI            | `http://localhost:15672`                                                |
| List queues                   | `docker exec -it rabbitmq rabbitmqadmin list queues`                    |
| List consumers                | `docker exec -it rabbitmq rabbitmqadmin list consumers`                 |
| Clear stale connections       | `docker exec -it rabbitmq rabbitmqctl close_all_connections "stale"`    |
| Delete a queue                | `docker exec -it rabbitmq rabbitmqadmin delete queue name=<queue_name>` |
| Restart broker                | `docker restart rabbitmq`                                               |
