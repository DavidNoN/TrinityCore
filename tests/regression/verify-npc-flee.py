#!/usr/bin/env python3
"""Compile production NPC flee code with isolated world-state fixtures.

Run with Python 3 and a C++20 compiler. These regressions exercise combat and
quest state transitions; a full worldserver build checks the real engine APIs.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


HERE = Path(__file__).resolve().parent
CORE = HERE.parents[1]


def extract(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.DOTALL)
    depth = 0
    for token in tokens.finditer(source, opening):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[start:token.end()]
    raise ValueError("Unclosed production definition: " + signature)


def compile_fixture(name, substitutions):
    fixture = (HERE / (name + ".cpp")).read_text()
    for marker, source in substitutions.items():
        if fixture.count(marker) != 1:
            raise ValueError("Expected exactly one insertion marker: " + marker)
        fixture = fixture.replace(marker, source)
    with tempfile.TemporaryDirectory(prefix="npc-flee-regression-") as directory:
        source = Path(directory) / (name + ".cpp")
        binary = Path(directory) / name
        source.write_text(fixture)
        subprocess.run([
            *shlex.split(os.environ.get("CXX", "c++")), "-std=c++20",
            "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined",
            "-fno-sanitize-recover=all", str(source), "-o", str(binary),
        ], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True)
    return {"fixture": name, "exit_code": result.returncode, "output": result.stdout + result.stderr}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", type=Path, default=CORE)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    definitions = {
        "src/server/game/Entities/Creature/Creature.cpp": [
            "bool AssistDelayEvent::Execute", "void Creature::DoFleeToGetAssistance",
            "void Creature::CallAssistance", "bool Creature::CanAssistTo",
        ],
        "src/server/game/Movement/MotionMaster.cpp": ["void MotionMaster::MoveSeekAssistance("],
        "src/server/game/Movement/MovementGenerators/PointMovementGenerator.cpp": [
            "void AssistanceMovementGenerator::Finalize",
        ],
    }
    methods = [extract((args.core / path).read_text(), signature)
               for path, signatures in definitions.items() for signature in signatures]
    notifiers = (args.core / "src/server/game/Grids/Notifiers/GridNotifiers.h").read_text()
    predicates = "namespace Trinity {\n" + "\n".join(
        extract(notifiers, "class " + name) + ";"
        for name in ["AnyAssistCreatureInRangeCheck", "NearestAssistCreatureInCreatureRangeCheck"]
    ) + "\n}"
    results = [compile_fixture("flee-assistance", {
        "// INSERT_PRODUCTION_METHODS_HERE": "\n\n".join(methods),
        "// INSERT_PRODUCTION_PREDICATES_HERE": predicates,
    })]
    try:
        shadowmoon = (args.core / "src/server/scripts/Outland/zone_shadowmoon_valley.cpp").read_text()
        durotar = (args.core / "src/server/scripts/Kalimdor/zone_durotar.cpp").read_text()
        scripts = [extract(shadowmoon, "enum BlackBloodOfDraenor") + ";",
                   extract(shadowmoon, "struct npc_black_blood_of_draenor") + ";",
                   extract(durotar, "enum WaywardPlainstrider") + ";",
                   extract(durotar, "struct npc_wayward_plainstrider") + ";"]
        results.append(compile_fixture("npc-flee", {
            "// INSERT_PRODUCTION_SCRIPTS_HERE": "\n\n".join(scripts),
        }))
    except ValueError as error:
        results.append({"fixture": "npc-flee", "exit_code": 1,
                        "output": "FAIL production flee/merge quest scripts are missing: " + str(error) + "\n"})
    report = {"method": "Production definitions with isolated world fixtures and UBSan",
              "live_client_combat_verified": False, "results": results}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + "\n")
    for result in results:
        print(result["output"], end="")
    return int(any(result["exit_code"] for result in results))


if __name__ == "__main__":
    raise SystemExit(main())
