import json
import pathlib

import unreal


PROJECT_ROOT = pathlib.Path(r"F:\AI_WorkSpeace\ai")
REPORT_PATH = PROJECT_ROOT / "Tools" / "asset_reorg_repair_report.json"


REPAIR_MAP = {
    "/Game/GameAssets/Grid_Materials/Materials/Instances/MI_BlockOut_Grass_Neon_2": "/Game/GameAssets/Grid_Materials/Materials/Master/MM_Grid",
    "/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Demo/Scene/Materials/MI_Vefects_Grid_01": "/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Demo/Scene/Materials/MM_Vefects_Grid",
    "/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Demo/Scene/Materials/MI_Vefects_Grid_02": "/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Demo/Scene/Materials/MM_Vefects_Grid",
    "/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/MI_VFX_Lush_Galaxy_Shader_01": "/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/M_VFX_Lush_Galaxy_Shader",
    "/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/MI_VFX_Lush_Galaxy_Shader_01_Inverted": "/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/M_VFX_Lush_Galaxy_Shader",
    "/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/MI_VFX_Lush_Galaxy_Shader_02": "/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/M_VFX_Lush_Galaxy_Shader",
    "/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/MI_VFX_Lush_Galaxy_Shader_03": "/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/M_VFX_Lush_Galaxy_Shader",
    "/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_3": "/Game/GameAssets/HologramShader/Materials/HologramShader/M_Hologram",
    "/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_5": "/Game/GameAssets/HologramShader/Materials/HologramShader/M_Hologram",
    "/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_8": "/Game/GameAssets/HologramShader/Materials/HologramShader/M_Hologram",
    "/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_10": "/Game/GameAssets/HologramShader/Materials/HologramShader/M_Hologram",
    "/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_12": "/Game/GameAssets/HologramShader/Materials/HologramShader/M_Hologram",
    "/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_18": "/Game/GameAssets/HologramShader/Materials/HologramShader/M_Hologram",
    "/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_21": "/Game/GameAssets/HologramShader/Materials/HologramShader/M_Hologram",
}


def main() -> None:
    editor_asset_library = unreal.EditorAssetLibrary
    fixed: list[dict[str, str]] = []
    skipped: list[dict[str, str]] = []
    failed: list[dict[str, str]] = []

    for asset_path, parent_path in REPAIR_MAP.items():
        asset = unreal.load_asset(asset_path)
        parent_asset = unreal.load_asset(parent_path)

        if asset is None:
            skipped.append({"asset": asset_path, "reason": "asset_missing"})
            continue

        if parent_asset is None:
            failed.append({"asset": asset_path, "reason": "parent_missing", "parent": parent_path})
            continue

        try:
            asset.set_editor_property("parent", parent_asset)
            editor_asset_library.save_loaded_asset(asset, only_if_is_dirty=False)
            fixed.append({"asset": asset_path, "parent": parent_path})
        except Exception as exc:
            failed.append({"asset": asset_path, "reason": str(exc), "parent": parent_path})

    REPORT_PATH.write_text(
        json.dumps(
            {
                "fixed": fixed,
                "skipped": skipped,
                "failed": failed,
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )
    unreal.log_warning(f"ASSET_REORG_REPAIR_WRITTEN={REPORT_PATH}")


if __name__ == "__main__":
    main()
