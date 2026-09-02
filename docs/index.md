# QRM&CI Overview

## Where QRM&CI sits in MQSS

**MQSS** — the _Munich Quantum Software Stack_ — is the broader stack this repository belongs to. It
provides compilation and runtime infrastructure for on-premise and remote quantum devices, supports
modern optimization techniques and high-level quantum programming abstractions, and is designed to
work across deployment models ranging from stand-alone installations to tightly integrated HPC
environments.

Quantum Resource Manager and Compiler Infrastructure (QRM&CI) is the HPC-side runtime layer in this
repository. It moves quantum tasks through backend selection, compilation, scheduling, and device
submission, with RabbitMQ queues between stages and QDMI at the device boundary.

![The Munich Quantum Software Stack, from user-facing frontends down to the devices, with QRM&CI as the HPC-side runtime layer](MQSS-overall-color.png){html: width=80%}

## Components of the QRM&CI pipeline

QRM&CI treats task handling as a staged pipeline:

- **Selector** chooses a backend that is online and compatible with the task.
- **Compiler** rewrites the task for the selected backend.
- **Scheduler** orders ready work by policy before submission.
- **Submitter** sends jobs to the selected device through QDMI and returns results.

Each stage receives work, processes it, and forwards it. In standalone mode all stages run in one
process. In distributed mode the selector and worker processes hand work to each other over RabbitMQ.

![The QRM&CI pipeline: selector, compiler, scheduler, and submitter, connected by RabbitMQ queues, with QDMI at the device boundary](QRM-detail-mqss-style.png){html: width=60%}

_QOffload appears in the diagram to show where work enters the pipeline. It is an MQSS frontend
interface, not part of this repository — see "What is not QRM&CI" below._

## What is not QRM&CI

This repository does not implement every MQSS component around the pipeline:

- **QOffload and other MQSS frontend surfaces** sit upstream of this repository and hand work into
  QRM&CI.
- **QDMI devices and drivers** are external runtime dependencies that the submitter talks to.
- **The MQSS compiler, scheduler, submitter, and messaging libraries** are linked dependencies of
  `qrmci`; this repository integrates them into one runtime path.

## Deployment modes

QRM&CI currently ships two deployment modes:

- **Standalone:** one process runs selection, compilation, scheduling, and submission together.
- **Distributed:** one selector process receives tasks and chooses backends, and worker processes
  compile, schedule, submit, and publish backend status for the selector to consume.

The distributed topology is one selector paired with one or more workers. The selector keeps its
backend registry from the status messages the workers publish, so adding a worker adds a backend the
selector can choose from.

## Library and daemons

The pipeline logic lives in a static library, `qrmci`, built from `include/qrmci/*.h` and
`src/*.cpp`. All three shipped binaries — `qrmcid-standalone`, `qrmcid-distributed-selector`, and
`qrmcid-distributed-worker` — are a single `main.cpp` each, linked against that library. There is no
plugin system: composing the pipeline differently means writing a fourth entrypoint the same way.

If the shipped daemons fit your needs, deploy them ([Getting Started](getting-started.md)). If you
need a different workflow, compiler, or scheduling strategy, write your own entrypoint against
`qrmci` ([Build custom QRM&CI workflow](using-the-library.md)).

## Which reader are you?

| I want to...                   | You are here if...                                              | Start here                                           |
| ------------------------------ | --------------------------------------------------------------- | ---------------------------------------------------- |
| Deploy QRM&CI                  | you want the shipped standalone or distributed daemons          | [Getting Started](getting-started.md)                |
| Build custom QRM&CI workflow   | you need a different workflow, compiler, or scheduling strategy | [Build custom QRM&CI workflow](using-the-library.md) |
| Contribute to QRM&CI itself    | you are editing code in this repository                         | [Development Guide](development-guide.md)            |
| Look up symbols and interfaces | you need the exact public types and function contracts          | [API Reference](api-reference.md)                    |

## Terminology

- **task** - an `mqss::QuantumTask` moving through the pipeline
- **backend** - a device snapshot with a name, qubit count, status, and supported circuit formats
- **backend registry** - the per-process set of known backends, with liveness tracked by a
  time-to-live and a publish interval
- **backend status** - the `mqss::Backend` snapshot a submitter-owning process publishes so that a
  selector without a device connection can keep its registry fresh
- **stage** - one step in the pipeline, such as selection or submission
- **queue** - a RabbitMQ queue carrying tasks, results, or backend status between stages
- **driver/device** - the QDMI endpoint a submitter-owning process talks to
- **ready job** - a scheduled task the scheduler has released for submission
- **in flight** - submitted to the device but not yet collected
- **cancellation result** - the result a daemon sends back in place of a `mqss::QuantumResult` when a
  task cannot be selected, compiled, or executed
