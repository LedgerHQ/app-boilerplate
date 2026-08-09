# AI-Assisted Ledger App Framework

This project provides a complete **AI-driven ecosystem** to accelerate the development, testing, and review of secure Ledger embedded applications.
It uses a Multi-Agent architecture to ensure that every feature is not just "coded", but also **secure, tested (Ragger/Speculos), and documented**.

## The Squad (Skills)

Each Markdown file defines a specialized expert agent:

| File | Role | Responsibility |
| :--- | :--- | :--- |
| **`ORCHESTRATOR.md`** | **Project Manager** | **The Single Entry Point.** Coordinates the workflow and enforces the pipeline. |
| `SKILL_LEDGER_C.md` | Dev Agent | Secure C coding, static memory, SDK optimization. |
| `SKILL_LEDGER_QA.md` | QA Agent | Ragger tests, Docker builds, Golden Snapshots. |
| `SKILL_LEDGER_DOC.md` | Doc Agent | Technical documentation (APDU, Mermaid) extracted from code. |
| `SKILL_LEDGER_REVIEW.md`| Review Agent | The Gatekeeper. Audits the Code/Test/Doc trinity. |

All skill files live in **`.github/prompts/`**. The Orchestrator references them via `#file:SKILL_LEDGER_C.md`, etc.

---

## How to Use

### 1. Prerequisites
* **VS Code** with an AI extension (GitHub Copilot, Cursor, or **Cline**).
* **Docker** installed (required for builds and tests).
* The `.md` skill files present in `.github/prompts/`.

### 2. The Golden Rule
**Do not talk to the Dev or QA agents directly.**
You interact **exclusively** with the Orchestrator.

1.  Open your AI Chat (or "Composer" / "Copilot Edits" mode).
2.  **Reference the `@ORCHESTRATOR.md` file** in your context (e.g. `#file:ORCHESTRATOR.md`).
3.  Type your high-level request.

### 3. Prompt Examples

**The Functional Request:**
> `@ORCHESTRATOR.md` Add a new APDU command to derive a Bitcoin Testnet address and display it on screen for validation.

**The Robustness Test ("Moon Mode"):**
> `@ORCHESTRATOR.md` Add the GO_TO_MOON command (INS 0x42). It takes a u64 amount, multiplies it by 1000, and displays the result with the message "WAGMI".

---

## The Automatic Workflow

Once you submit the prompt, the Orchestrator triggers this loop:

1.  **PHASE 1 (Dev):** Generates C Code (`src/`).
2.  **PHASE 2 (QA):** Generates Python Tests & Snapshots (`tests/`) using Docker.
3.  **PHASE 3 (Doc):** Updates Documentation (`doc/`).
4.  **PHASE 4 (Review):** Full Audit.
    * ❌ **FAIL:** The Orchestrator forces the agents to fix the issues.
    * ✅ **PASS:** The feature is ready to merge.
