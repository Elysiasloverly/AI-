import json
import pathlib

import unreal


PROJECT_ROOT = pathlib.Path(r"F:\AI_WorkSpeace\ai")
AUDIT_PATH = PROJECT_ROOT / "Tools" / "asset_reorg_audit.json"
REPORT_PATH = PROJECT_ROOT / "Tools" / "asset_reorg_move_report.json"
DEST_ROOT = "/Game/GameAssets"


def main() -> None:
    report = json.loads(AUDIT_PATH.read_text(encoding="utf-8"))
    used_packages: list[str] = sorted(report["used_packages"])

    editor_asset_library = unreal.EditorAssetLibrary
    moved: list[dict[str, str]] = []
    skipped: list[dict[str, str]] = []
    failed: list[dict[str, str]] = []

    editor_asset_library.make_directory(DEST_ROOT)

    for source_path in used_packages:
        if source_path.startswith(f"{DEST_ROOT}/"):
            skipped.append({"source": source_path, "reason": "already_in_destination"})
            continue

        if not editor_asset_library.does_asset_exist(source_path):
            skipped.append({"source": source_path, "reason": "source_missing"})
            continue

        relative_path = source_path[len("/Game/"):]
        destination_path = f"{DEST_ROOT}/{relative_path}"
        destination_dir = destination_path.rsplit("/", 1)[0]
        editor_asset_library.make_directory(destination_dir)

        if editor_asset_library.does_asset_exist(destination_path):
            skipped.append({"source": source_path, "reason": "destination_exists", "destination": destination_path})
            continue

        success = editor_asset_library.rename_asset(source_path, destination_path)
        if not success and "/Maps/" in source_path and not editor_asset_library.does_asset_exist(destination_path):
            success = editor_asset_library.duplicate_asset(source_path, destination_path)
        if success:
            moved.append({"source": source_path, "destination": destination_path})
        else:
            failed.append({"source": source_path, "destination": destination_path})

    REPORT_PATH.write_text(
        json.dumps(
            {
                "moved_count": len(moved),
                "skipped_count": len(skipped),
                "failed_count": len(failed),
                "moved": moved,
                "skipped": skipped,
                "failed": failed,
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )
    unreal.log_warning(f"ASSET_REORG_MOVE_WRITTEN={REPORT_PATH}")


if __name__ == "__main__":
    main()
