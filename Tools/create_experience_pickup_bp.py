"""创建经验球蓝图资产，并把默认外观搭好。

用法（项目根目录）：
    UnrealEditor-Cmd.exe F:\AI_WorkSpeace\baka\Baka.uproject ^
        -run=pythonscript -script="Tools/create_experience_pickup_bp.py" ^
        -unattended -nopause -nullrhi -nosplash

产物：/Game/BP/World/BP_ExperiencePickup
父类：ARogueExperiencePickup（C++ 侧只有碰撞、池化、复制与磁吸逻辑，没有任何视觉组件）

生成的组件结构（全部是蓝图自有组件，组件面板里可以直接删、换、改材质）：
    Collision（C++ 根组件：拾取球体）
    └── VisualRoot            视觉挂点，悬浮动画作用在它身上
        ├── BodyMesh          主体球
        ├── GlowMesh          外层辉光球
        └── GlowLight         点光源（默认关闭，想让球照亮地面就打开）
    Floating                  漂浮组件，目标为 VisualRoot

已存在时不覆盖：脚本只打印路径，避免冲掉你在蓝图里调好的外观。
"""

import unreal

DEST = "/Game/BP/World"
NAME = "BP_ExperiencePickup"
ASSET_PATH = "{}/{}".format(DEST, NAME)

MESH_SPHERE = "/Game/GameAssets/StarterContent/Shapes/Shape_Sphere.Shape_Sphere"
MAT_BODY = "/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_C_Inst.M_Glow_C_Inst"
MAT_GLOW = ("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/"
            "MI_VFX_Lush_Galaxy_Shader_01.MI_VFX_Lush_Galaxy_Shader_01")


def log(message):
    unreal.log("[ExperiencePickup] {}".format(message))


def warn(message):
    unreal.log_warning("[ExperiencePickup] {}".format(message))


def try_set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as error:
        warn("设置 {} 失败（{}）：{}".format(prop, obj.get_name(), error))
        return False


class BlueprintBuilder(object):
    def __init__(self, blueprint):
        self.blueprint = blueprint
        self.subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        roots = self.subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
        if not roots:
            raise RuntimeError("gather subobject data failed")
        self.root_handle = roots[0]

    def find_handle(self, template):
        for handle in self.subsystem.k2_gather_subobject_data_for_blueprint(self.blueprint):
            obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(handle)
            if obj is not None and obj == template:
                return handle
        return None

    def add(self, parent_handle, component_class, name):
        """加一个组件，返回 (句柄, 普通组件与模板)，失败返回 (None, None)。"""
        params = unreal.AddNewSubobjectParams()
        params.set_editor_property("parent_handle", parent_handle)
        params.set_editor_property("new_class", component_class)
        params.set_editor_property("blueprint_context", self.blueprint)

        result = self.subsystem.add_new_subobject(params)
        if isinstance(result, tuple):
            new_handle, fail_reason = result[0], result[1]
        else:
            new_handle, fail_reason = result, ""

        if new_handle is None:
            warn("创建 {} 失败：{}".format(name, fail_reason))
            return None, None

        data = self.subsystem.k2_find_subobject_data_from_handle(new_handle)
        template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)

        try:
            self.subsystem.rename_subobject(new_handle, name)
        except Exception as error:
            warn("组件改名 {} 失败：{}".format(name, error))

        return new_handle, template


def setup_sphere(template, scale, material_path):
    mesh = unreal.load_asset(MESH_SPHERE)
    if mesh is not None:
        try_set(template, "static_mesh", mesh)

    material = unreal.load_asset(material_path)
    if material is not None:
        try_set(template, "override_materials", [material])

    try_set(template, "relative_scale3d", unreal.Vector(scale, scale, scale))
    try_set(template, "cast_shadow", False)
    try:
        template.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    except Exception as error:
        warn("关闭 {} 的碰撞失败：{}".format(template.get_name(), error))


def bind_floating_target(floating, component_name):
    """把漂浮组件的目标指到某个组件。

    FComponentReference 的两个字段在不同版本上可编辑性不一样：
    优先写 ComponentProperty（BP 组件名就是同名属性），失败再退回 PathToComponent。
    """
    for prop in ("component_property", "path_to_component"):
        reference = unreal.ComponentReference()
        try:
            reference.set_editor_property(prop, component_name)
            floating.set_editor_property("floating_target", reference)
            log("漂浮目标已绑定（{} = {}）".format(prop, component_name))
            return True
        except Exception as error:
            warn("绑定漂浮目标失败（{}）：{}".format(prop, error))
    warn("漂浮目标未绑定，请在 Floating 组件细节里手动选 {}".format(component_name))
    return False


def log_components(blueprint, subsystem):
    """打印蓝图里的 SCS 节点，方便确认组件结构符合预期。

    注意不要用 CDO 的 GetComponentsByClass：它拿不到 SCS 组件，只会返回 C++ 根组件。
    """
    seen = set()
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is None:
            continue
        # 同一组件会同时以"节点句柄"和"组件模板句柄"出现，按名字去重。
        name = obj.get_name()
        if name in seen:
            continue
        seen.add(name)
        log("node: {} [{}]".format(name, obj.get_class().get_name()))


def main():
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        log("already exists: {}（不覆盖你在蓝图里做的外观修改）".format(ASSET_PATH))
        existing = unreal.load_asset(ASSET_PATH)
        if existing is not None:
            log_components(existing, unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem))
        return

    if not unreal.EditorAssetLibrary.does_directory_exist(DEST):
        unreal.EditorAssetLibrary.make_directory(DEST)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.RogueExperiencePickup)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, DEST, unreal.Blueprint, factory)
    if blueprint is None:
        raise RuntimeError("failed to create {}".format(ASSET_PATH))

    builder = BlueprintBuilder(blueprint)

    # 视觉挂点：悬浮动画作用在它身上，子组件跟着一起上下浮动。
    visual_handle, visual_root = builder.add(builder.root_handle, unreal.SceneComponent, "VisualRoot")
    if visual_handle is None:
        raise RuntimeError("创建 VisualRoot 失败")

    body_handle, body = builder.add(visual_handle, unreal.StaticMeshComponent, "BodyMesh")
    if body is not None:
        setup_sphere(body, 0.24, MAT_BODY)

    glow_handle, glow = builder.add(visual_handle, unreal.StaticMeshComponent, "GlowMesh")
    if glow is not None:
        setup_sphere(glow, 0.46, MAT_GLOW)

    light_handle, light = builder.add(visual_handle, unreal.PointLightComponent, "GlowLight")
    if light is not None:
        try_set(light, "intensity", 780.0)
        # LightColor 是 FColor，且 Python 侧参数顺序是 (r, g, b, a)，必须用具名参数。
        try_set(light, "light_color", unreal.Color(r=36, g=184, b=255, a=255))
        try_set(light, "attenuation_radius", 125.0)
        try_set(light, "source_radius", 12.0)
        try_set(light, "soft_source_radius", 16.0)
        try_set(light, "use_inverse_squared_falloff", False)
        try_set(light, "cast_shadows", False)
        try_set(light, "visible", False)  # 想让它照亮地面就把这个打开

    # 漂浮组件：默认目标是 VisualRoot；没绑上就在组件细节里重新选一次。
    floating_handle, floating = builder.add(builder.root_handle, unreal.RogueFloatingComponent, "Floating")
    if floating is not None:
        try_set(floating, "float_amplitude", 10.0)
        try_set(floating, "float_period", 3.14)
        bind_floating_target(floating, "VisualRoot")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_asset(ASSET_PATH, False)
    log("created {}".format(ASSET_PATH))
    log_components(blueprint, builder.subsystem)


main()
