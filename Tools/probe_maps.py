"""只读检查两张地图的加载状态与关卡内容（帮助判断哪张是现役地图、是否存在断链）。

用法：
    UnrealEditor-Cmd.exe F:\\AI_WorkSpeace\\ai\\ai.uproject ^
        -run=pythonscript -script="Tools/probe_maps.py" ^
        -unattended -nopause -nullrhi -nosplash
"""
import unreal

MAPS = [
    "/Game/GameAssets/StarterContent/Maps/Minimal_Default",
    "/Game/Map/Minimal_Default",
]


def log(message):
    unreal.log("[MapProbe] {}".format(message))


for path in MAPS:
    log("===== {} =====".format(path))
    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map(path)
    except Exception as exc:  # noqa: BLE001
        log("load failed: {}".format(exc))
        continue
    if world is None:
        log("load returned None")
        continue

    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    log("actors: {}".format(len(actors)))
    class_counts = {}
    broken = []
    for actor in actors:
        if actor is None:
            continue
        try:
            label = actor.get_actor_label()
            cls = actor.get_class()
            class_name = cls.get_name() if cls else "None"
        except Exception:  # noqa: BLE001
            broken.append("actor with unreadable class")
            continue
        class_counts[class_name] = class_counts.get(class_name, 0) + 1
        if cls is None:
            broken.append(label)
    for name, count in sorted(class_counts.items(), key=lambda item: -item[1]):
        log("  {} x{}".format(name, count))
    if broken:
        log("BROKEN ACTORS: {}".format(broken))
    log("player_start count: {}".format(class_counts.get("PlayerStart", 0)))
