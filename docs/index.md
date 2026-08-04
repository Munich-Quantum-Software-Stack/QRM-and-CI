# Quantum Resource Manager and Compiler Infrastructure (QRM&CI)

The Quantum Resource Manager and Compiler Infrastructure (QRM&CI) connects scheduling,
compilation, submission, and result handling for MQSS workflows. The QRM&CI daemon orchestrates a staged workflow that receives quantum tasks, selects a backend,
compiles the task, schedules it, submits it to QDMI, and routes results back to the caller.

# Concepts

## Enablement Components

- Data: Quantum Task etc
- Communication layer
- Backend Interface
- Frontend Interface

## Enablement Functions

- QRMCI Library API

## Integrated Components

- QOffload
- Selector
- Compiler
- Scheduler
- Submitter

### Getting started

Use [Getting Started](getting-started.md) to configure and deploy QRM&CI in your own environment.

### Development

Use [Development Guide](development-guide.md) for build, testing, and contribution workflows.

### API Reference

Detailed [API documentation](api-reference.md).
