# Documentation

This folder describes the interface of the application: the APDUs exchanged with the host.
It has two entry points:

| Document | Content | Read it to |
| --- | --- | --- |
| [APDU.md](APDU.md) | **Static** reference: transport, command summary, encoding of each command, transaction format, status words | Implement or check a single command |
| [FLOWS.md](FLOWS.md) | **Dynamic** description: for each feature, the sequence of APDUs between the host, the OS, the application and the user, with sequence diagrams and device screens | Integrate a feature end to end |

## Keeping it up to date

When forking the Boilerplate, replace the content of both documents with your own application:

- A new or modified command is described in [APDU.md](APDU.md): a row in the command summary and its own section.
- Each feature using it gets a flow in [FLOWS.md](FLOWS.md). Prefer a generated flow: tag the functional test
  of the feature with `@pytest.mark.flow(name, variant)` and add a `<!-- BEGIN GENERATED FLOW name -->` block,
  so that the flow is kept up to date by the test itself.
- The encoding of a command is only described in [APDU.md](APDU.md): [FLOWS.md](FLOWS.md) links to it instead
  of repeating it.
- Diagrams are written in [Mermaid](https://mermaid.js.org/syntax/sequenceDiagram.html), which GitHub renders
  natively.
