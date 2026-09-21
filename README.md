<div align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="./docs/_static/mqss_logo_dark.svg" width="20%">
    <img src="./docs/_static/mqss_logo.svg" width="20%">
  </picture>
</div>

# Quantum Resource Manager and Compiler Infrastructure (QRM&CI)

<!-- [DOXYGEN MAIN] -->

Quantum Resource Manager and Compiler Infrastructure (QRM&CI) is a core component of the Munich
Quantum Software Stack (MQSS). It connects classical high-performance computing (HPC) systems with
quantum resources, and provides the runtime machinery that moves work between the two.

QRM&CI combines compilation, optimization, scheduling, and device submission into a single pipeline.
It applies both target-agnostic and target-specific optimization passes, schedules quantum tasks
across the available devices, and returns the results of executed circuits to the HPC application
that submitted them — giving hybrid HPC/quantum workloads one execution path from task to result.

<!-- [DOXYGEN MAIN] -->

<div align="center">
    <img src="./docs/_static/MQSS-overall-with-QRM-inset.png" alt="QRM&CI" width="85%">
</div>

## Components of QRM&CI

1. **QOffload** hands work into QRM&CI from the MQSS frontend interfaces.
2. **Selector** chooses the backend a quantum task should run on.
3. **Compiler** applies target-agnostic and target-specific passes, including optimization and
   transpilation.
4. **Scheduler** orders ready work according to the configured scheduling policy before submission.
5. **Submitter** sends the job to the chosen quantum device through the **Quantum Device Management
   Interface (QDMI)** and returns the result.

Selector, compiler, scheduler, and submitter behavior are implemented in this repository's code and
entrypoints. QOffload is shown here only to make the boundary visible.

## Getting Started

<div align="center">
<a href="https://munich-quantum-software-stack.github.io/QRM-and-CI/">
  <img style="min-width: 100px !important; width: 40%;" src="https://img.shields.io/badge/documentation-blue?style=for-the-badge&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA0NDggNTEyIj48IS0tIUZvbnQgQXdlc29tZSBGcmVlIDYuNi4wIGJ5IEBmb250YXdlc29tZSAtIGh0dHBzOi8vZm9udGF3ZXNvbWUuY29tIExpY2Vuc2UgLSBodHRwczovL2ZvbnRhd2Vzb21lLmNvbS9saWNlbnNlL2ZyZWUgQ29weXJpZ2h0IDIwMjQgRm9udGljb25zLCBJbmMuLS0+PHBhdGggZmlsbD0iI2ZmZmZmZiIgZD0iTTk2IDBDNDMgMCAwIDQzIDAgOTZMMCA0MTZjMCA1MyA0MyA5NiA5NiA5NmwyODggMCAzMiAwYzE3LjcgMCAzMi0xNC4zIDMyLTMycy0xNC4zLTMyLTMyLTMybDAtNjRjMTcuNyAwIDMyLTE0LjMgMzItMzJsMC0zMjBjMC0xNy43LTE0LjMtMzItMzItMzJMMzg0IDAgOTYgMHptMCAzODRsMjU2IDAgMCA2NEw5NiA0NDhjLTE3LjcgMC0zMi0xNC4zLTMyLTMyczE0LjMtMzIgMzItMzJ6bTMyLTI0MGMwLTguOCA3LjItMTYgMTYtMTZsMTkyIDBjOC44IDAgMTYgNy4yIDE2IDE2cy03LjIgMTYtMTYgMTZsLTE5MiAwYy04LjggMC0xNi03LjItMTYtMTZ6bTE2IDQ4bDE5MiAwYzguOCAwIDE2IDcuMiAxNiAxNnMtNy4yIDE2LTE2IDE2bC0xOTIgMGMtOC44IDAtMTYtNy4yLTE2LTE2czcuMi0xNiAxNi0xNnoiLz48L3N2Zz4=" alt="Documentation" />
  </a>
</div>

| I want to...                 | You are here if...                                              | Start here                                     |
| ---------------------------- | --------------------------------------------------------------- | ---------------------------------------------- |
| Deploy QRM&CI                | you want the shipped standalone or distributed daemons          | [Getting Started](docs/getting-started.md)     |
| Build custom QRM&CI workflow | you need a different workflow, compiler, or scheduling strategy | [Using the Library](docs/using-the-library.md) |
| Contribute to QRM&CI itself  | you are editing code in this repository                         | [Development Guide](docs/development-guide.md) |

## License

QRM&CI is released under the Apache License v2.0 with LLVM Exceptions. See
[LICENSE](https://github.com/Munich-Quantum-Software-Stack/QRM-and-CI/blob/develop/LICENSE) for the full
terms. Unless stated otherwise, contributions to this project are assumed to be licensed under the
same terms.

<!-- [DOXYGEN FAQ] -->

## Contact

Please use the public GitHub channels whenever possible:
[issues](https://github.com/Munich-Quantum-Software-Stack/QRM-and-CI/issues),
[discussions](https://github.com/Munich-Quantum-Software-Stack/QRM-and-CI/discussions), and
[pull requests](https://github.com/Munich-Quantum-Software-Stack/QRM-and-CI/pulls). This keeps questions,
proposals, and implementation details visible and easy to follow.
