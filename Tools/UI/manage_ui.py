"""Current UI authoring entry, executed by UE Python.

Default: compile-check saved WidgetBlueprints in memory; write only a QA report.
Explicit -RogueUIRebuild=/Game/.../WBP_Name rebuilds ONE supported widget's
native starter layout after backing up its package. Current saved WBP assets are
the authoritative appearance; a starter layout does not reproduce manual styling.
"""
from pathlib import Path
from datetime import datetime
import json, re, shutil, unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
COMMAND = unreal.SystemLibrary.get_command_line()
MATCH = re.search(r'-RogueUIRebuild=(?:"([^"\n]+)"|(\S+))', COMMAND)
TARGET = (MATCH.group(1) or MATCH.group(2)) if MATCH else None
BUILDERS = {
    'WBP_MainMenu': ('RogueWidgetBuilder', 'build_main_menu'),
    'WBP_PlayerVitals': ('RogueWidgetBuilder', 'build_player_vitals'),
    'WBP_EnemyHealthBar': ('RogueWidgetBuilder', 'build_enemy_health_bar'),
    'WBP_DamageFlash': ('RogueWidgetBuilder', 'build_damage_flash'),
    'WBP_NightTransition': ('RogueWidgetBuilder', 'build_night_transition'),
    'WBP_VictoryScreen': ('RogueWidgetBuilder', 'build_victory_screen'),
}
REGISTRY = unreal.AssetRegistryHelpers.get_asset_registry()
REGISTRY.search_all_assets(True)
REPORT = {'action': 'rebuild-one' if TARGET else 'verify', 'widgets': {}, 'backup': None}
if TARGET:
    if not TARGET.startswith('/Game/UMG/') or '.' in TARGET or '..' in TARGET:
        raise ValueError('Use an exact /Game/UMG/... package path.')
    NAME = TARGET.rsplit('/', 1)[-1]
    if NAME not in BUILDERS:
        raise ValueError('No starter builder registered for ' + NAME)
    BP = unreal.load_asset(TARGET)
    if not isinstance(BP, unreal.WidgetBlueprint):
        raise ValueError('Target is not an existing WidgetBlueprint: ' + TARGET)
    PACKAGE = ROOT / 'Content' / (TARGET.removeprefix('/Game/') + '.uasset')
    BACKUP = ROOT / 'Saved/QA/UIBackups' / datetime.now().strftime('%Y%m%d-%H%M%S-%f') / PACKAGE.name
    BACKUP.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(PACKAGE, BACKUP)
    if PACKAGE.read_bytes() != BACKUP.read_bytes():
        raise RuntimeError('Backup verification failed')
    CLASS, METHOD = BUILDERS[NAME]
    if not getattr(getattr(unreal, CLASS), METHOD)(BP):
        raise RuntimeError('Builder failed; no save attempted')
    if not unreal.RogueEditorAssetUtils.validate_blueprint(BP):
        raise RuntimeError('Compile failed; no save attempted')
    if not unreal.RogueEditorAssetUtils.save_asset(BP):
        raise RuntimeError('Save failed')
    REPORT['backup'] = str(BACKUP)
    REPORT['widgets'][TARGET] = True
else:
    for DATA in REGISTRY.get_assets_by_path('/Game/UMG', True):
        if str(DATA.asset_class_path.asset_name) == 'WidgetBlueprint':
            BP = DATA.get_asset()
            REPORT['widgets'][str(DATA.package_name)] = bool(BP and unreal.RogueEditorAssetUtils.validate_blueprint(BP))
OUT = ROOT / 'Saved/QA/UI/last-report.json'
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
if not REPORT['widgets'] or not all(REPORT['widgets'].values()):
    raise RuntimeError('UI verification failed; inspect ' + str(OUT))
unreal.log('UI authoring completed: ' + str(OUT))
