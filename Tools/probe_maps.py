"""Read-only validation of the project's main menu and two combat maps. Does not save assets."""
from pathlib import Path
import unreal,json,re
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
expected={'/Game/Map/MainMenu','/Game/Map/Battle_Night','/Game/Map/Battle_Day'}
maps={'/Game/'+str(p.relative_to(root/'Content').with_suffix('')).replace('\\','/') for p in (root/'Content').rglob('*.umap')}
assert maps==expected,'Unexpected map set: '+str(sorted(maps))
settings=unreal.get_default_object(unreal.GameMapsSettings)
def configured_map(name):
 value=settings.get_editor_property(name).export_text()
 paths=re.findall(r'/Game/[A-Za-z0-9_./]+',value)
 assert len(paths)==1,(name,value)
 return paths[0].split('.')[0]
assert configured_map('game_default_map')=='/Game/Map/MainMenu'
assert configured_map('editor_startup_map')=='/Game/Map/Battle_Day'
for name in ['RogueHUD','RogueMainMenuHUD']:
 cdo=unreal.get_default_object(unreal.load_class(None,'/Script/ai.'+name))
 # MainMenuHUD deliberately falls back to its base HUD's config at initialization.
 value=cdo.get_editor_property('single_player_map_path')
 assert value in ('','/Game/Map/Battle_Day'),(name,value)
assert unreal.get_default_object(unreal.RogueHUD).get_editor_property('single_player_map_path')=='/Game/Map/Battle_Day'
assert unreal.get_default_object(unreal.RogueRoomSubsystem).get_editor_property('battle_map')=='/Game/Map/Battle_Day'
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
for name in sorted(expected):
 assert level.load_level(name),name
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 mode=world.get_world_settings().get_editor_property('default_game_mode')
 required='/Script/ai.RogueMainMenuGameMode' if name.endswith('MainMenu') else '/Game/BP/Framework/BP_RogueGameMode.BP_RogueGameMode_C'
 assert mode and mode.get_path_name()==required,(name,str(mode))
 world=None  # Release the editor world before loading the next map.
unreal.log('PROJECT_MAPS_VERIFIED: '+json.dumps(sorted(maps)))
