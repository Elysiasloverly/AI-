import unreal

world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Map/MainMenu")
actors = unreal.EditorLevelLibrary.get_all_level_actors()
print("MAP_ACTORS", [(a.get_actor_label(), a.get_class().get_name(), list(a.tags)) for a in actors])

for a in actors:
    if isinstance(a, unreal.CameraActor):
        comp = a.get_component_by_class(unreal.CameraComponent)
        print("CAMERA", a.get_actor_label(), "fov", comp.get_editor_property("field_of_view") if comp else None)
    if isinstance(a, unreal.SceneCapture2D):
        cap = a.get_component_by_class(unreal.SceneCaptureComponent2D)
        print("CAPTURE", a.get_actor_label(), "rt", cap.get_editor_property("texture_target") if cap else None,
              "every", cap.get_editor_property("capture_every_frame") if cap else None)

rt = unreal.EditorAssetLibrary.load_asset("/Game/UMG/Menus/MainMenu/RT_MenuBackground")
print("RT", rt, "size", (rt.get_editor_property("size_x"), rt.get_editor_property("size_y")) if rt else None)
