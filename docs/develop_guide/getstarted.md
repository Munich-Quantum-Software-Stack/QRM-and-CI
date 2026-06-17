# Getting Started

## Prerequisites

Clone the project:

```bash
git clone https://github.com/akshay9594/QRM.git \
       /workspaces/QRM
cd /workspaces/QRM
git checkout 382758da6a992339f39e9e8f5b135f6dc0ba46cf
```

If using docker, RUN the commands:

```sh
docker build -t mqss-qrm-dev -f .devcontainer/Dockerfile .
docker run --rm -it \
  -v "$PWD":/workspaces/QRM \
  -w /workspaces/QRM \
  mqss-qrm-dev \
  bash
```

Note: The project root is at ```/workspaces/QRM```

## Building and Installing the project

To build, RUN the build script in project root by running the command:

```sh
./build.sh
```

This should download and install all the required dependencies and create
the project targets all within the ```build``` directory.

Following key targets should be generated:

```
build/MQSSCompiler
build/MQSSScheduler
build/tests/test-hpc-qrm-simple
```

## Testing the installation via RabbitMQ

Important: Please refer to [rabbitmq-setup](docs/rabbitmq.md) on how to setup RabbitMQ
          with your docker dev container (mqss-qrm-dev).

Once the rabbitmq docker container and your dev container are on the same network and can communicate
via Rabbitmq, run the following script that runs all the target executables mentioned above.
The test case here is ```test-hpc-qrm-simple```.

```sh
./start.sh
```

The ```test-hpc-qrm-simple``` reads a ```c++``` circuit input file from ```benchmarks/``` directory and
creates a quantum-task. It then forwards this quantum-task to the ```Scheduler``` which the forwards
it to the ```MQSS Compiler```. The compiler runs various optimizations and translations on the circuit
within the quantum-task, updates the task with the new circuit (in ```QIR```) format and sends the task
back to ```test-hpc-qrm-simple``` which simply prints the received QIR circuit to its log file.

Next, we will add support for a ```Submitter``` module that will submit the quantum-task to a real
quantum device.
