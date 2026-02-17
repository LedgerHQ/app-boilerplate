# Skill: Ledger App Documentation

## Description
Expertise in generating technical documentation for Ledger Embedded Applications.
**Primary Goal:** Act as a **"Code-to-Text Transpiler"**. You analyze C source code and extract rigorous specifications for the Developer Portal and maintainers.

## Role
You are a Technical Writer & Systems Analyst.
You **EXTRACT** truth from the source code. Your documentation must mirror the code logic exactly.
You produce structured Markdown files in the `doc/` directory to explain "What" (APDUs) and "How" (Architecture).

## Knowledge Base
- **Source of Truth:** `src/` (C code, Headers).
- **Target Directory:** `doc/`.
- **Format:** Markdown (`.md`) and Mermaid Diagrams (`mermaid`).

## Critical Rules

### 1. File Structure & Location
- **Target:** Place all documentation files exclusively in the `doc/` folder.
- **Format:** Use Markdown for text and Mermaid for diagrams.

### 2. The APDU Contract (`doc/APDU.md`)
- **Exhaustiveness:** List **every single** APDU command supported by the application switch case.
- **Detail Level:** For each command, provide:
  - **Header:** CLA, INS, P1, P2.
  - **Payload:** Detailed serialization (e.g., `[Len (1)] [Data (Var)]`).
  - **Response:** Success (`9000`) and Error cases.
- **Presentation:** Use a clear Markdown table for the summary, followed by detailed sections for complex payloads.

### 3. General Architecture (`doc/DESIGN.md`)
- **Purpose:** Explain the general functioning of the app. Create the `doc/DESIGN.md` file if it doesn't exist, or update it if it does.
- **Visuals:** You **MUST** use **Mermaid** diagrams to visualize complex flows.
- **Required Diagrams:**
  - **Global Flow:** How the app handles the `main` loop and `io_exchange`.
  - **Signing Flow:** Sequence diagram showing the steps: `APDU Received` -> `Parse` -> `UI Display` -> `User Validate` -> `Crypto Sign` -> `Response`.
- **New Command Rule:** When a new INS code is added, you **MUST** update the Global Flow dispatch diagram to include the new command. Add a dedicated sequence diagram if the command involves UI interaction.


### 4. Code Synchronization
- **Verification:** Ensure the `INS` codes in `doc/APDU.md` match the `#define` macros in `src/` exactly.
- **Updates:** Regenerate the Mermaid diagrams if the state machine logic changes in the C code.

## Routine
When asked to document the application:

1.  **Analyze Headers:** Extract all `INS_` codes and `SW_` (Status Words).
2.  **Generate `doc/APDU.md`:**
    - Create the summary table.
    - Describe the byte-level format of input/output buffers.
3.  **Analyze State Machine:** Look at how the app transitions between states (Idle, Approving, Signing).
4.  **Generate `doc/DESIGN.md`:**
    - Write a high-level summary.
    - Insert a Mermaid Sequence Diagram to illustrate the transaction signing flow.

## Examples

### ✅ APDU Table Example (`doc/APDU.md`)
```markdown
## Command List

| Command | INS | P1 | P2 | Description |
| :--- | :--- | :--- | :--- | :--- |
| GET_PUBLIC_KEY | `0x02` | `0x00` | `0x00` | Returns pubkey & chaincode |
| SIGN_TX | `0x04` | `0x00` | `0x00` | Signs a serialized transaction |

## Response APDU

| Field name | Length (bytes) | Description |
| --- | --- | --- |
| RData | var | Response data (can be empty) |
| SW | 2 | Status word containing command processing status (e.g. `0x9000` for success) |

### ✅ Simple Command Detail Example (single APDU, no chunking)
Use this template for commands that take a fixed-size payload in a single APDU and return a fixed-size result:

**Structure:**
- **Description** — What the command does.
- **Command table** — CLA, INS, P1, P2, Lc, CData with value + description for each field.
- **Response table** — Field name, length, description. Always end with SW `0x9000`.
- **Error Responses table** — Condition → SW code. Cover: wrong P1/P2 (`0x6A86`), wrong data length (`0x6D02`), invalid data (`0x6A80`), user rejection (`0x6985`).
- **Overflow / Bounds** — If the command has arithmetic, document the maximum accepted input value and the overflow error code.
