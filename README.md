<!----------------------------------------------------------------------------
Copyright 2024 Munich Quantum Software Stack Project

Licensed under the Apache License, Version 2.0 with LLVM Exceptions (the
"License"); you may not use this file except in compliance with the License.
You may obtain a copy of the License at

TODO

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
License for the specific language governing permissions and limitations under
the License.

SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
---------------------------------------------------------------------------->

<div align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="./docs/_static/mqss_logo_dark.svg" width="20%">
    <img src="./docs/_static/mqss_logo.svg" width="20%">
  </picture>
</div>

# MQSS Quantum Resource Manager (MQSS-QRM)

<!-- [DOXYGEN MAIN] -->
This repository hosts the Quantum Resource Manager (QRM), a component of the Munich Quantum Software
Stack (MQSS) that bridges classical and quantum resources within a high-Performance and Quantum
Computing (HPCQC) environment. The QRM acts as a robust runtime framework, seamlessly orchestrating
operations between HPC and quantum resources.

<!-- [DOXYGEN MAIN] -->
<div align="center">
  <img style="min-width: 200px !important; width: 30%;" src="https://img.shields.io/badge/documentation-blue?style=for-the-badge&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA0NDggNTEyIj48IS0tIUZvbnQgQXdlc29tZSBGcmVlIDYuNi4wIGJ5IEBmb250YXdlc29tZSAtIGh0dHBzOi8vZm9udGF3ZXNvbWUuY29tIExpY2Vuc2UgLSBodHRwczovL2ZvbnRhd2Vzb21lLmNvbS9saWNlbnNlL2ZyZWUgQ29weXJpZ2h0IDIwMjQgRm9udGljb25zLCBJbmMuLS0+PHBhdGggZmlsbD0iI2ZmZmZmZiIgZD0iTTk2IDBDNDMgMCAwIDQzIDAgOTZMMCA0MTZjMCA1MyA0MyA5NiA5NiA5NmwyODggMCAzMiAwYzE3LjcgMCAzMi0xNC4zIDMyLTMycy0xNC4zLTMyLTMyLTMybDAtNjRjMTcuNyAwIDMyLTE0LjMgMzItMzJsMC0zMjBjMC0xNy43LTE0LjMtMzItMzItMzJMMzg0IDAgOTYgMHptMCAzODRsMjU2IDAgMCA2NEw5NiA0NDhjLTE3LjcgMC0zMi0xNC4zLTMyLTMyczE0LjMtMzIgMzItMzJ6bTMyLTI0MGMwLTguOCA3LjItMTYgMTYtMTZsMTkyIDBjOC44IDAgMTYgNy4yIDE2IDE2cy03LjIgMTYtMTYgMTZsLTE5MiAwYy04LjggMC0xNi03LjItMTYtMTZ6bTE2IDQ4bDE5MiAwYzguOCAwIDE2IDcuMiAxNiAxNnMtNy4yIDE2LTE2IDE2bC0xOTIgMGMtOC44IDAtMTYtNy4yLTE2LTE2czcuMi0xNiAxNi0xNnoiLz48L3N2Zz4=" alt="Documentation" />
  </a>
</div>

## Getting Started

### Prerequisites

Clone the project:

```bash
git clone https://github.com/Munich-Quantum-Software-Stack/QRM.git <your-local-path>
cd <your-local-path>
git checkout DA-QRM
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

### Building and Installing the project

To build, RUN the build script in project root by running the command:

```sh
make build
```

This should download and install all the required dependencies and create
the project targets all within the ```build``` directory.

Following key targets should be generated:

```
build/MQSSSubmitter
build/MQSSCompiler
build/MQSSScheduler
build/tests/test-hpc-qrm-simple
```

If changes are made to the source files, rebuild using:</br>

```sh
make build
```

### Testing the installation via RabbitMQ

**Important**: Please refer to [rabbitmq-setup](docs/develop_guide/rabbitmq.md) on how to setup RabbitMQ
          with your docker dev container (mqss-qrm-dev).

Once the rabbitmq docker container and your dev container are on the same network and can communicate
via Rabbitmq, run the following script that runs all the target executables mentioned above.
The test case here is ```test-hpc-qrm-simple``` which creates the quantum task.

```sh
make invoke
```

The ```test-hpc-qrm-simple``` reads a ```c++``` circuit input file from ```benchmarks/``` directory and
creates a quantum-task. It then forwards this quantum-task to the ```Scheduler``` which the forwards
it to the ```MQSS Compiler```. The compiler runs various optimizations and translations on the circuit
within the quantum-task, updates the task with the new circuit (in ```QIR```) format and sends the task
to the ```MQSS Submitter``` which creates a QDMI job. The example QDMI device and QDMI driver found
in the QDMI [repository](https://github.com/Munich-Quantum-Software-Stack/QDMI/tree/develop/examples) are
used to create the job. The job is then submitted to the same example device and results are received by
the Submitter and forwarded back to ```test-hpc-qrm-simple```.

### Checking Logs

The logs of each of the component are generated within ```QRM/logs``` directory.
This directory is automatically created during the build process and populated during
execution.

The logs are:</br>

```
Compiler : Compiler.log
Scheduler : Scheduler.log
Submiiter : Submitter.log
test case: Test-Case.log
```
