from pathlib import Path
import unreal,json
work=Path(__file__).parent
folder='/Game/UMG/Menus/Multiplayer'
unreal.EditorAssetLibrary.make_directory(folder)
created=[]
def widget(name,parent):
    path=folder+'/'+name
    existing=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if existing:return existing,False
    factory=unreal.WidgetBlueprintFactory()
    factory.set_editor_property('parent_class',parent)
    bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.WidgetBlueprint,factory)
    assert bp,path
    created.append(path)
    return bp,True
rows=[]
for name in ('WBP_RoomEntry','WBP_LobbyPlayerEntry'):
    bp,new=widget(name,unreal.RogueRoomEntryWidgetBase)
    if new:
        assert unreal.RogueWidgetBuilder.build_room_entry(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
    rows.append(bp.generated_class())
for page,name in enumerate(('WBP_MultiplayerMenu','WBP_RoomBrowser','WBP_Lobby')):
    bp,new=widget(name,unreal.RogueMultiplayerWidgetBase)
    if not new:continue
    assert unreal.RogueWidgetBuilder.build_multiplayer_page(bp,page)
    cdo=unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('Page',page)
    if page:cdo.set_editor_property('EntryWidgetClass',rows[page-1])
    bp.modify()
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
(work/'widgets-result.json').write_text(json.dumps({'created':created,'existing_styles_preserved':True},ensure_ascii=False,indent=2),encoding='utf-8')
