<div align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="./docs/_static/mqss_logo_dark.svg" width="20%">
    <img src="./docs/_static/mqss_logo.svg" width="20%">
  </picture>
</div>

# Quantum Resource Manager and Compiler Infrastructure (QRM&CI) of the MQSS

<!-- [DOXYGEN MAIN] -->

This repository contains the Quantum Resource Manager and Compiler Infrastructure (QRM&CI), a core
component of the Munich Quantum Software Stack (MQSS). It connects classical high-performance
computing (HPC) systems with quantum resources and provides the runtime machinery needed to move
work efficiently between both environments.

QRM&CI combines compilation, optimization, scheduling, and device submission in a single workflow.
It applies both target-agnostic and target-specific optimization passes, schedules quantum tasks
across available devices, and returns results from executed circuits back to the originating HPC
application.

The result is a unified execution path for hybrid HPC and quantum workloads: tasks are prepared,
routed, executed on real quantum devices, and reintegrated into the broader application flow with as
little friction as possible.

<!-- [DOXYGEN MAIN] -->
<div align="center">
<a href="https://munich-quantum-software-stack.github.io/QRM">
  <img style="min-width: 200px !important; width: 30%;" src="https://img.shields.io/badge/documentation-blue?style=for-the-badge&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA0NDggNTEyIj48IS0tIUZvbnQgQXdlc29tZSBGcmVlIDYuNi4wIGJ5IEBmb250YXdlc29tZSAtIGh0dHBzOi8vZm9udGF3ZXNvbWUuY29tIExpY2Vuc2UgLSBodHRwczovL2ZvbnRhd2Vzb21lLmNvbS9saWNlbnNlL2ZyZWUgQ29weXJpZ2h0IDIwMjQgRm9udGljb25zLCBJbmMuLS0+PHBhdGggZmlsbD0iI2ZmZmZmZiIgZD0iTTk2IDBDNDMgMCAwIDQzIDAgOTZMMCA0MTZjMCA1MyA0MyA5NiA5NiA5NmwyODggMCAzMiAwYzE3LjcgMCAzMi0xNC4zIDMyLTMycy0xNC4zLTMyLTMyLTMybDAtNjRjMTcuNyAwIDMyLTE0LjMgMzItMzJsMC0zMjBjMC0xNy43LTE0LjMtMzItMzItMzJMMzg0IDAgOTYgMHptMCAzODRsMjU2IDAgMCA2NEw5NiA0NDhjLTE3LjcgMC0zMi0xNC4zLTMyLTMyczE0LjMtMzIgMzItMzJ6bTMyLTI0MGMwLTguOCA3LjItMTYgMTYtMTZsMTkyIDBjOC44IDAgMTYgNy4yIDE2IDE2cy03LjIgMTYtMTYgMTZsLTE5MiAwYy04LjggMC0xNi03LjItMTYtMTZ6bTE2IDQ4bDE5MiAwYzguOCAwIDE2IDcuMiAxNiAxNnMtNy4yIDE2LTE2IDE2bC0xOTIgMGMtOC44IDAtMTYtNy4yLTE2LTE2czcuMi0xNiAxNi0xNnoiLz48L3N2Zz4=" alt="Documentation" />
  </a>
</div>

## FAQ

<!-- [DOXYGEN FAQ] -->

### What is MQSS?

**MQSS** stands for _Munich Quantum Software Stack_. It is a project of the _Munich Quantum Valley
(MQV)_ initiative and is jointly developed by the _Leibniz Supercomputing Centre (LRZ)_ and the
Chairs for _Design Automation (CDA)_ and _Computer Architecture and Parallel Systems (CAPS)_ at TUM.

MQSS provides compilation and runtime infrastructure for on-premise and remote quantum devices. It
supports modern optimization techniques, enables high-level quantum programming abstractions, and is
designed to work across several deployment models, from stand-alone installations to tightly
integrated HPC environments.

Within MQV, a concrete instance of MQSS is deployed at the LRZ as a unified access point to the
available quantum devices. It supports several access paths, including a web portal, command-line
access with web credentials, and hybrid workflows that integrate directly with LRZ HPC systems.

<div align="center">
    <img src="./docs/_static/MQSS-overall.png" width="100%">
</div>

### What is the **Quantum Resource Manager and Compiler Infrastructure (QRM&CI)**?

QRM&CI is the HPC-side runtime layer of MQSS. It is composed of the following blocks:

<div align="center">
    <img src="./docs/_static/QRM-detail.png" width="100%">
</div>

1. **QOffload** provides the interface between QRM&CI and the MQSS frontend.
2. **Platform Selector** chooses the device on which each quantum task will run.
3. **Compiler** receives quantum tasks and applies target-agnostic and target-specific passes,
   including optimization and transpilation.
4. **Scheduler** assigns each task to a selected device according to the configured scheduling
   policy.
5. **Submitter** sends tasks to the chosen quantum device through the **Quantum Device Management
   Interface (QDMI)**.

The components interact as a data pipeline. Each stage receives jobs, processes them, and forwards
them to the next stage through FIFOs at the boundaries of the blocks. The overall design mirrors an
operating-system pipeline: components communicate through FIFO channels, and individual stages may
delegate work to execution threads to support parallel or concurrent processing.

### Where is the code?

The source code is publicly available on GitHub:
<https://github.com/Munich-Quantum-Software-Stack/QRM>

### Under which license is the **QRM&CI** released?

QRM&CI is released under the Apache License v2.0 with LLVM Exceptions. See [LICENSE](https://github.com/Munich-Quantum-Software-Stack/QRM/blob/develop/LICENSE) for the full terms. Unless stated otherwise, contributions to this project are assumed to be licensed under the same terms.

<!-- [DOXYGEN FAQ] -->

## Contact

Please use the public GitHub channels whenever possible:
[issues](https://github.com/Munich-Quantum-Software-Stack/QRM/issues),
[discussions](https://github.com/Munich-Quantum-Software-Stack/QRM/discussions), and
[pull requests](https://github.com/Munich-Quantum-Software-Stack/QRM/pulls). This keeps
questions, proposals, and implementation details visible and easy to follow.
