"""Render the flows recorded by flow_recorder.py into doc/FLOWS.md.

Usage, from tests/:

    pytest standalone --device flex --flows_dir standalone/flows -m flow
    python -m standalone.generate_flows --flows_dir standalone/flows [--check]

Each ``<!-- BEGIN GENERATED FLOW <name> -->`` / ``<!-- END GENERATED FLOW <name> -->`` block of
doc/FLOWS.md is replaced by the sequence diagram and the screens of the tests
tagged ``@pytest.mark.flow(name, ...)``. The text outside of these blocks is left untouched.

With ``--check``, nothing is written and the script fails if doc/FLOWS.md is not up to date.
"""

import argparse
import json
import re
import sys
from pathlib import Path

from application_client.boilerplate_command_sender import CLA, Errors, InsType

ROOT = Path(__file__).resolve().parents[2]
FLOWS_MD = ROOT / "doc" / "FLOWS.md"
SNAPSHOTS = ROOT / "tests" / "standalone" / "snapshots"
# Width of the screen thumbnails, in pixels
THUMB = 120

# Commands handled by the OS rather than by the application
OS_CLA = 0xB0
OS_COMMANDS = {0x01: "GET_APP_AND_VERSION", 0x06: "LOAD_CERTIFICATE"}

APP_COMMANDS = {ins.value: ins.name for ins in InsType}
STATUS_WORDS = {error.value: error.name for error in Errors}

BLOCK_NAMES = re.compile(r"<!-- BEGIN GENERATED FLOW ([\w-]+) -->")
ARROW = re.compile(r"^(\w)-{1,2}>>(\w):")
BLOCK = re.compile(
    r"(<!-- BEGIN GENERATED FLOW (?P<name>[\w-]+) -->\n).*?(<!-- END GENERATED FLOW (?P=name) -->)",
    re.DOTALL,
)


def command_name(cla: int, ins: int) -> str:
    if cla == OS_CLA:
        return OS_COMMANDS.get(ins, f"OS_{ins:02X}")
    if cla == CLA:
        return APP_COMMANDS.get(ins, f"INS_{ins:02X}")
    return f"CLA_{cla:02X}_INS_{ins:02X}"


def describe(step: dict, peer: str) -> list[str]:
    """Return the Mermaid lines of one recorded step."""
    if step["type"] == "command":
        apdu = bytes.fromhex(step["apdu"])
        cla, ins, p1, p2 = apdu[:4]
        data_len = len(apdu) - 5
        label = f"{command_name(cla, ins)} ({cla:02X} {ins:02X}) P1={p1:02X} P2={p2:02X}"
        if data_len > 0:
            label += f", {data_len} bytes"
        return [f"H->>{'O' if cla == OS_CLA else 'A'}: {label}"]
    if step["type"] == "response":
        sw = int(step["sw"], 16)
        label = step["sw"].upper()
        if sw != 0x9000:
            label += f" {STATUS_WORDS.get(sw, '')}".rstrip()
        if step["data"]:
            label = f"{len(step['data']) // 2} bytes, {label}"
        return [f"{peer}-->>H: {label}"]
    return [f"A->>U: {step['screen']} screens", f"U->>A: {step['action']}"]


def to_lines(steps: list[dict]) -> list[str]:
    lines: list[str] = []
    peer = "A"
    for step in steps:
        if step["type"] == "command":
            peer = "O" if bytes.fromhex(step["apdu"])[0] == OS_CLA else "A"
        lines += describe(step, peer)
    return lines


def commands_of(steps: list[dict]) -> set[str]:
    names = set()
    for step in steps:
        if step["type"] == "command":
            apdu = bytes.fromhex(step["apdu"])
            names.add(command_name(apdu[0], apdu[1]))
    return names


def trim(recording: dict) -> list[dict]:
    """Drop the setup exchanges recorded before the first occurrence of the 'start' command."""
    steps = recording["steps"]
    start = recording.get("start")
    if start is None:
        return steps
    for index, step in enumerate(steps):
        if step["type"] == "command":
            apdu = bytes.fromhex(step["apdu"])
            if command_name(apdu[0], apdu[1]) == start:
                return steps[index:]
    raise ValueError(f"{recording['test']}: command {start} not found")


def render_diagram(variants: list[tuple[str, list[dict], dict]]) -> list[str]:
    lines_per_variant = [to_lines(steps) for _, steps, _ in variants]

    # Exchanges shared by all variants are drawn once, the rest in an alt block
    common = 0
    while all(len(lines) > common for lines in lines_per_variant) and (
        len({lines[common] for lines in lines_per_variant}) == 1
    ):
        common += 1

    participants = {name for lines in lines_per_variant for line in lines for name in ARROW.match(line).groups()}
    out = ["```mermaid", "sequenceDiagram", "    participant H as Host"]
    if "O" in participants:
        out.append("    participant O as OS")
    out.append("    participant A as App")
    if "U" in participants:
        out.append("    actor U as User")
    out.append("")
    out += [f"    {line}" for line in lines_per_variant[0][:common]]
    if len(variants) > 1:
        for index, ((variant, _, _), lines) in enumerate(zip(variants, lines_per_variant, strict=True)):
            out.append(f"    {'alt' if index == 0 else 'else'} {variant}")
            out += [f"        {line}" for line in lines[common:]]
        out.append("    end")
    else:
        out += [f"    {line}" for line in lines_per_variant[0][common:]]
    out.append("```")
    return out


def render_screens(variant: str, steps: list[dict], device: str) -> list[str]:
    """Thumbnails of the snapshots taken during the user steps, in navigation order."""
    images = []
    for step in steps:
        if step["type"] != "user":
            continue
        for index, png in enumerate(sorted((SNAPSHOTS / device / step["snapshots"]).glob("*.png")), start=1):
            src = f"../tests/standalone/snapshots/{device}/{step['snapshots']}/{png.name}"
            images.append(
                f'<a href="{src}"><img src="{src}" alt="{step["screen"]} screen {index}" width="{THUMB}"></a>'
            )
    if not images:
        return []
    # Snapshot paths make these lines longer than the markdown line length limit
    return [
        "",
        "<details open>",
        f"<summary>{variant}: screens on {device.capitalize()}</summary>",
        "<!-- markdownlint-disable MD013 -->",
        "<p>",
        *images,
        "</p>",
        "<!-- markdownlint-enable MD013 -->",
        "</details>",
    ]


def render(recordings: list[dict], device: str) -> str:
    variants = [(rec["variant"], trim(rec), rec) for rec in recordings]
    out = render_diagram(variants)
    for variant, steps, _ in variants:
        out += render_screens(variant, steps, device)
    return "\n".join(out) + "\n"


def load(flows_dir: Path, device: str) -> dict[str, list[dict]]:
    flows: dict[str, list[dict]] = {}
    for path in sorted((flows_dir / device).glob("*.json")):
        recording = json.loads(path.read_text())
        flows.setdefault(recording["flow"], []).append(recording)
    for recordings in flows.values():
        recordings.sort(key=lambda recording: recording["location"])
    return flows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "--flows_dir", type=Path, default=Path("standalone/flows"), help="directory given to pytest --flows_dir"
    )
    parser.add_argument("--device", default="flex", help="device whose recordings and screens are documented")
    parser.add_argument("--check", action="store_true", help="fail if doc/FLOWS.md is not up to date")
    args = parser.parse_args()

    flows = load(args.flows_dir, args.device)
    original = FLOWS_MD.read_text()
    missing = []

    def replace(match: re.Match) -> str:
        name = match.group("name")
        if name not in flows:
            missing.append(name)
            return match.group(0)
        return match.group(1) + "\n" + render(flows[name], args.device) + "\n" + match.group(3)

    updated = BLOCK.sub(replace, original)
    for name in missing:
        print(f"error: no recording for flow '{name}', tag a test with @pytest.mark.flow('{name}', ...)")

    unused = set(flows) - set(BLOCK_NAMES.findall(original))
    for name in sorted(unused):
        print(f"warning: flow '{name}' is recorded but has no block in {FLOWS_MD.name}")

    documented = {step_name for recs in flows.values() for rec in recs for step_name in commands_of(trim(rec))}
    for name in sorted(set(APP_COMMANDS.values()) - documented):
        print(f"warning: {name} does not appear in any generated flow")

    if missing:
        return 1
    if args.check:
        if updated != original:
            print(f"error: {FLOWS_MD.relative_to(ROOT)} is not up to date, run python -m standalone.generate_flows")
            return 1
        print(f"{FLOWS_MD.relative_to(ROOT)} is up to date")
        return 0
    FLOWS_MD.write_text(updated)
    print(f"{FLOWS_MD.relative_to(ROOT)} updated: {', '.join(sorted(flows))}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
