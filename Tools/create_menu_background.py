"""Set up the 3D main-menu background: wall, lights, camera, capture camera, render target.

Usage:
  UnrealEditor-Cmd.exe Baka.uproject -run=pythonscript -script="Tools/create_menu_background.py" \
      -unattended -nopause -nullrhi -nosplash
"""
import unreal

MAP_PATH = "/Game/Map/MainMenu"
RT_DIR = "/Game/UMG/Menus/MainMenu"
RT_NAME = "RT_MenuBackground"
RT_PATH = "{}/{}".format(RT_DIR, RT_NAME)

RESULT = {}


def log(msg):
    unreal.log("[MenuBackground] {}".format(msg))


def all_actors():
    return list(unreal.EditorLevelLibrary.get_all_level_actors())


def find_actor(label):
    for actor in all_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def destroy_existing(label):
    actor = find_actor(label)
    if actor:
        unreal.EditorLevelLibrary.destroy_actor(actor)
        log("destroyed old {}".format(label))


def make_rt():
    if unreal.EditorAssetLibrary.does_asset_exist(RT_PATH):
        return unreal.EditorAssetLibrary.load_asset(RT_PATH)
    factory = unreal.TextureRenderTargetFactoryNew()
    rt = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        RT_NAME, RT_DIR, unreal.TextureRenderTarget2D, factory)
    if rt:
        rt.set_editor_property("size_x", 1280)
        rt.set_editor_property("size_y", 720)
        rt.set_editor_property("render_target_format", unreal.TextureRenderTargetFormat.RTF_RGBA8)
        rt.set_editor_property("clear_color", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
        unreal.EditorAssetLibrary.save_loaded_asset(rt)
        log("created render target {}".format(RT_PATH))
    return rt


def to_folder(actor):
    try:
        actor.set_folder_path("MainMenuBackground")
    except Exception as exc:  # noqa: BLE001
        log("folder failed {}: {}".format(actor.get_actor_label(), exc))


def spawn_camera(label, location, rotation, tag, auto_activate=False):
    destroy_existing(label)
    cam = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.CameraActor, location, rotation)
    cam.set_actor_label(label)
    to_folder(cam)
    cam.tags = [tag]
    try:
        cam.set_editor_property("auto_activate_for_player",
                                unreal.AutoActivateForPlayer.PLAYER0 if auto_activate
                                else unreal.AutoActivateForPlayer.DISABLED)
    except Exception as exc:  # noqa: BLE001
        log("auto_activate failed {}: {}".format(label, exc))
    cam_comp = cam.get_component_by_class(unreal.CameraComponent)
    if cam_comp:
        cam_comp.set_editor_property("field_of_view", 60.0)
    return cam


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    if world is None:
        print("MENU_BG FAILED no map")
        return

    # Wall
    destroy_existing("MenuWall")
    wall = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(900.0, 0.0, 600.0), unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
    wall.set_actor_label("MenuWall")
    to_folder(wall)
    wall.set_actor_scale3d(unreal.Vector(0.2, 24.0, 12.0))
    mesh = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    mat = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
    mesh_comp = wall.get_component_by_class(unreal.StaticMeshComponent)
    if mesh_comp and mesh:
        mesh_comp.set_static_mesh(mesh)
        if mat:
            mesh_comp.set_material(0, mat)
    log("wall ready")

    # Key light
    destroy_existing("MenuKeyLight")
    key = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(400.0, -400.0, 1000.0), unreal.Rotator(pitch=-50.0, yaw=0.0, roll=0.0))
    key.set_actor_label("MenuKeyLight")
    to_folder(key)
    key_comp = key.get_component_by_class(unreal.DirectionalLightComponent)
    if key_comp:
        key_comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        key_comp.set_editor_property("intensity", 6.0)
        key_comp.set_editor_property("light_color", unreal.Color(255, 245, 230, 255))
    log("key light ready")

    # Fill light so the wall is not pitch black before a material is assigned.
    destroy_existing("MenuFillLight")
    fill = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PointLight, unreal.Vector(400.0, 0.0, 500.0), unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
    fill.set_actor_label("MenuFillLight")
    to_folder(fill)
    fill_comp = fill.get_component_by_class(unreal.PointLightComponent)
    if fill_comp:
        fill_comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        fill_comp.set_editor_property("intensity", 8000.0)
        fill_comp.set_editor_property("attenuation_radius", 4000.0)
    log("fill light ready")

    # Main menu camera (HUD looks for the MenuCamera tag).
    spawn_camera("MenuCamera", unreal.Vector(0.0, 0.0, 450.0), unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0),
                 "MenuCamera", auto_activate=True)
    # Second camera aimed at the wall, for a different framing / future cut.
    spawn_camera("MenuCamera_Close", unreal.Vector(0.0, 300.0, 380.0), unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0),
                 "MenuCameraAlt", auto_activate=False)

    # SceneCapture camera + render target (for using the same wall in WBP later).
    destroy_existing("MenuBackgroundCapture")
    capture = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SceneCapture2D, unreal.Vector(0.0, 0.0, 450.0), unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
    capture.set_actor_label("MenuBackgroundCapture")
    to_folder(capture)
    capture.tags = ["MenuCapture"]
    rt = make_rt()
    cap_comp = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    if cap_comp and rt:
        cap_comp.set_editor_property("texture_target", rt)
        cap_comp.set_editor_property("capture_every_frame", True)
        try:
            cap_comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        except Exception as exc:  # noqa: BLE001
            log("capture_source: {}".format(exc))
        log("scene capture ready -> {}".format(RT_PATH))

    unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH)
    RESULT["actors"] = [a.get_actor_label() for a in all_actors()]
    RESULT["rt"] = RT_PATH
    print("MENU_BG " + str(RESULT))


main()
