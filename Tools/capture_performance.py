"""Repeatable rendered performance capture; never saves gameplay assets or the real progression slot.

Requires a compiled Development Editor module with Rogue.Perf.Capture.
    python Tools/capture_performance.py --label after
Raw CSVs, warm-up screenshots and process logs are written below Saved/Profiling/RoguePerformance.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import subprocess


def main() -> None:
    project = Path(__file__).resolve().parents[1] / "Baka.uproject"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", required=True, help="Capture group, e.g. before or after")
    parser.add_argument("--ue", type=Path, default=Path(r"C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe"))
    parser.add_argument("--output", type=Path)
    parser.add_argument("--repeat", type=int, default=3)
    parser.add_argument("--warmup", type=float, default=15)
    parser.add_argument("--seconds", type=float, default=20)
    parser.add_argument("--scenes", nargs="+", choices=["idle","day200","night400"], default=["idle","day200","night400"])
    parser.add_argument("--batch-mode", choices=["current","component","batch"], default="current")
    parser.add_argument("--paired", action="store_true", help="Alternate component/batch within each repetition for an A/B comparison")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]+", args.label):
        parser.error("label must contain only letters, numbers, underscores or hyphens")
    if args.paired and args.batch_mode != "current":
        parser.error("--paired selects both paths; do not also set --batch-mode")
    if args.repeat < 1 or args.warmup < 2 or args.seconds < 3:
        parser.error("repeat >= 1, warmup >= 2, seconds >= 3 required")
    folder = (args.output or project.parent / "Saved/Profiling/RoguePerformance" / args.label).resolve()
    folder.mkdir(parents=True, exist_ok=True)
    settings = {"idle":("Battle_Day",0,0),"day200":("Battle_Day",200,4),"night400":("Battle_Night",400,20)}
    for scene in args.scenes:
        map_name,enemies,talismans=settings[scene]
        for repeat in range(1,args.repeat+1):
            modes=(["component","batch"] if repeat%2 else ["batch","component"]) if args.paired else [args.batch_mode]
            for mode in modes:
                label=f"{args.label}_{scene}_{mode}_{repeat}"
                if (folder / f"{label}.csv").exists():
                    raise SystemExit(f"Refusing to overwrite {label}.csv; use another label/output directory")
                capture=f'Rogue.Perf.Capture {label} {enemies} {talismans} {args.warmup} {args.seconds} "{folder.as_posix()}"'
                commands="t.MaxFPS 0,r.VSync 0,r.GPUCsvStatsEnabled 1,"
                if mode!="current":
                    commands+=f"Rogue.Projectiles.BatchEnemyMovement {int(mode=='batch')},"
                command=[str(args.ue),str(project),f"/Game/Map/{map_name}","-game","-RenderOffscreen","-windowed","-ResX=1920","-ResY=1080","-ForceRes","-NoVSync","-unattended","-nosound","-nop4","-SCCProvider=None","-NoSplash","-UTF8Output","-RogueProgressionSlot=PerfIsolated20261101",f"-abslog={folder / label}.log",f"-ExecCmds={commands}{capture}"]
                (folder/f"{label}.command.json").write_text(json.dumps(command,ensure_ascii=False,indent=2),encoding="utf-8")
                print(f"Capturing {label}...",flush=True)
                with (folder/f"{label}.console.log").open("w",encoding="utf-8") as log:
                    process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
                    try:
                        code=process.wait(timeout=args.warmup+args.seconds+150)
                    except subprocess.TimeoutExpired:
                        process.kill();process.wait()
                        raise SystemExit(f"Capture timed out: {label}")
                if code or not (folder/f"{label}.csv").exists():
                    raise SystemExit(f"Capture failed ({code}): {label}; inspect its log")
    print(f"Complete: {folder}")


if __name__ == "__main__":
    main()
