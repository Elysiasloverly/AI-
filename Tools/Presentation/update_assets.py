"""Apply presentation-only edits through UE. Run with the editor closed; installer backs up first."""
from pathlib import Path
import json, math, random, traceback, unreal
W=Path(__file__).parent
CAT=json.loads((W/'catalog.json').read_text(encoding='utf-8'))
LEVEL=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTORS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EDIT=unreal.MaterialEditingLibrary
REPORT={'passed':False,'maps':{},'created':[]}

def setp(o,**values):
 for k,v in values.items():o.set_editor_property(k,v)
def color(r,g,b):return unreal.LinearColor(r,g,b,1)
def save(a):assert unreal.RogueEditorAssetUtils.save_asset(a),a.get_path_name()
def key(v):
 # UE Python枚举显示为 <RogueUpgradeType.PROJECTILE_COUNT: 9>，DT导出为ProjectileCount。
 raw=str(v).split('::')[-1].split('.')[-1].split(':')[0].strip('<> ')
 normalized=raw.replace('_','').casefold()
 return next((name for name in CAT['titles'] if name.replace('_','').casefold()==normalized),raw)
def num(v):return format(float(v),'.4g')
def translated(s):
 for old,new in sorted(CAT['translations'].items(),key=lambda p:-len(p[0])):s=s.replace(old,new)
 return s

def card_text(t,r):
 m=float(r['Magnitude']);n=num(m);p=num(m*100)
 fixed={
  'MaxHealth':f'最大生命 +{n}，并立即回复新增上限对应的生命。',
  'MoveSpeed':f'移动速度 +{n} 厘米/秒，提高走位速度。',
  'ArmorCapacity':f'最大护甲 +{n}，并立即回复新增上限对应的护甲；同时重新开始护甲恢复。',
  'DashCooldown':f'冲刺冷却缩短 {n} 秒，受最短冷却限制；立即清除当前冲刺冷却。',
  'AttackPower':f'攻击力 +{n}。各武器按自身倍率获得伤害加成；灼光的基础伤害和每跳递增也会提高。',
  'AttackSpeed':'缩短武器攻击间隔，加快灼光伤害结算；银刃获得额外转速。各武器按自身倍率生效。',
  'PickupRadius':f'经验球吸取半径 +{n} 厘米，从更远处吸收经验。',
  'Recovery':f'每秒额外回复 {n} 点生命，提高持续作战能力。',
  'AttackRange':'扩大武器索敌范围，并按各武器倍率提高银刃环绕半径、爆炸半径与激光折射距离。',
  'ProjectileSpeed':'提高寒弹、冰焰球与霜玉的飞行速度，各武器按自身倍率生效。',
  'Armor':f'伤害减免提高 {p} 个百分点，受减伤上限限制。',
  'ExperienceGain':f'经验获取倍率增加 {p} 个百分点，相同经验球带来更多经验。',
  'ProjectileCount':f'每轮灵弹弹幕增加 {n} 枚寒弹；连发轮数由冰符连诵提升。',
  'TalismanCount':f'获得灵符·归心；已持有时增加 {n} 张环绕灵符，每张独立出击并返回。',
  'TalismanPierce':f'每张灵符单次出击可穿刺的不同敌人数 +{n}；默认命中 1 名后返回。',
  'TalismanRepeat':f'每张灵符单次出击对每名敌人的穿刺次数 +{n}；每次造成完整伤害。',
  'ScytheCount':f'获得银刃；已持有时，增加 {n} 把环绕刀刃。',
  'RocketCount':f'首次获得冰焰·陨星，从高空召唤冰焰球斜坠爆炸，地面蓝焰减速并持续造成少量伤害；已持有时，每轮额外召唤 {n} 颗。',
  'LaserCount':f'获得星符·流光；已持有时，每轮增加 {n} 束蓝白激光。',
  'HellTowerCount':f'获得焰符·灼光；已持有时，可同时锁定的不同敌人增加 {n} 名。',
  'MortarCount':f'获得冰符·霜玉；已持有时，每轮增加 {n} 枚抛射冰球。',
  'LaserRefraction':f'每束流光的折射次数 +{n}；命中后可跳向附近敌人，折射段按武器配置的倍率结算伤害。',
  'MortarBlastRadius':f'霜玉爆炸半径 +{n} 厘米，扩大落点周围的伤害区域。',
  'RocketBlastRadius':f'冰焰球爆炸半径 +{n} 厘米，同时增大球体、焰尾与地面蓝焰的影响范围。',
  'HellTowerDamageRamp':f'灼光每跳伤害递增值 +{n}；连续锁定同一目标越久，伤害增长越快。',
  'ScytheSize':f'银刃外观和伤害范围增加基础尺寸的 {p}%；环绕距离不变。',
  'ProjectileBurst':f'寒弹每轮攻击额外连射 {n} 次，每次发出完整弹幕；首次选取从单发变为 {num(1+m)} 连发。'
 }
 if t=='ScytheLifesteal':
  first=float(r.get('FirstMagnitude',-1));first=.2 if first<0 else first
  step=m if m>1e-4 else .05
  return f'银刃造成伤害时回复生命。首次吸血率 {num(first*100)}%，再次选择 +{num(step*100)} 个百分点，受吸血率上限限制。'
 return fixed[t] if t in fixed else translated(r.get('Description',''))

def table(path):
 a=unreal.load_asset(path);assert a,path
 return a,json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(a))
def no_text(rows,fields):return [{k:v for k,v in r.items() if k not in fields} for r in rows]
def protected_actor(a):
 d={'transform':a.get_actor_transform().export_text(),'class':a.get_class().get_path_name(),'components':[]}
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  mesh=c.get_editor_property('static_mesh')
  d['components'].append({'name':c.get_name(),'mesh':mesh.get_path_name() if mesh else None,'transform':c.get_relative_transform().export_text(),'materials':[x.get_path_name() if x else None for x in c.get_materials()],'collision':str(c.get_collision_enabled())})
 return d

def node_rules(n):
 # Export the struct without reading the legacy None enum through Python.
 text=n.export_text();assert text.startswith('(') and text.endswith(')')
 fields=[];start=1;depth=0;quoted=False;escaped=False
 for i,ch in enumerate(text[1:-1],1):
  if quoted:
   if escaped:escaped=False
   elif ch=='\\':escaped=True
   elif ch=='"':quoted=False
  elif ch=='"':quoted=True
  elif ch=='(':depth+=1
  elif ch==')':depth-=1
  elif ch==',' and depth==0:fields.append(text[start:i]);start=i+1
 fields.append(text[start:-1])
 return [f for f in fields if not f.strip().startswith(('Title=','Description='))]

def atmosphere_material():
 path='/Game/GameAssets/Environment/Atmosphere/M_ForestMotes'
 assert not unreal.EditorAssetLibrary.does_asset_exist(path),'Atmosphere material already exists; inspect previous report before retrying.'
 a=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_ForestMotes','/Game/GameAssets/Environment/Atmosphere',unreal.Material,unreal.MaterialFactoryNew());assert a
 setp(a,blend_mode=unreal.BlendMode.BLEND_ADDITIVE,shading_model=unreal.MaterialShadingModel.MSM_UNLIT,two_sided=False)
 def node(cls,x,y):return EDIT.create_material_expression(a,cls,x,y)
 def wire(x,y,pin):assert EDIT.connect_material_expressions(x,'',y,pin)
 tint=node(unreal.MaterialExpressionVectorParameter,-600,-300);setp(tint,parameter_name='Tint',default_value=color(1,.62,.18))
 energy=node(unreal.MaterialExpressionScalarParameter,-600,-160);setp(energy,parameter_name='Energy',default_value=2.4)
 mul=node(unreal.MaterialExpressionMultiply,-300,-230);wire(tint,mul,'A');wire(energy,mul,'B');assert EDIT.connect_material_property(mul,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 fresnel=node(unreal.MaterialExpressionFresnel,-600,40);setp(fresnel,exponent=1.8,base_reflect_fraction=0.)
 inverse=node(unreal.MaterialExpressionOneMinus,-350,40);wire(fresnel,inverse,'')
 opacity=node(unreal.MaterialExpressionMultiply,-100,40);setp(opacity,const_b=.4);wire(inverse,opacity,'A');assert EDIT.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
 time=node(unreal.MaterialExpressionTime,-600,260);pos=node(unreal.MaterialExpressionWorldPosition,-600,420)
 drift=node(unreal.MaterialExpressionCustom,-130,300)
 custom_inputs=[]
 for input_name in ['T','P']:
  item=unreal.CustomInput();item.set_editor_property('input_name',input_name);custom_inputs.append(item)
 setp(drift,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,code='float phase=P.x*.003+P.y*.005; return float3(9*sin(T*.27+phase),7*cos(T*.23+phase),12*sin(T*.41+phase));',inputs=custom_inputs)
 wire(time,drift,'T');wire(pos,drift,'P');assert unreal.RogueEditorAssetUtils.connect_world_position_offset(a,drift)
 EDIT.recompile_material(a);save(a);REPORT['created'].append(path)
 out={}
 for name,tint,energy in [('Day',color(1,.68,.22),1.5),('Night',color(.38,.83,.70),3.)]:
  mi=unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_ForestMotes_'+name,'/Game/GameAssets/Environment/Atmosphere',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew());assert mi
  EDIT.set_material_instance_parent(mi,a);EDIT.set_material_instance_vector_parameter_value(mi,'Tint',tint);EDIT.set_material_instance_scalar_parameter_value(mi,'Energy',energy);EDIT.update_material_instance(mi);save(mi)
  out[name]=mi;REPORT['created'].append(mi.get_path_name().split('.')[0])
 return out

try:
 weapons,wrows=table('/Game/Data/Weapons/DT_WeaponConfig');wb=no_text(wrows,{'DisplayName'})
 for r in wrows:
  if r['Name'] in CAT['weapons']:r['DisplayName']=CAT['weapons'][r['Name']]['name']
 assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(weapons,json.dumps(wrows,ensure_ascii=False));save(weapons)
 assert no_text(table('/Game/Data/Weapons/DT_WeaponConfig')[1],{'DisplayName'})==wb,'Weapon mechanics changed'
 upgrades,urows=table('/Game/Data/Upgrades/DT_UpgradeDefinitions');ub=no_text(urows,{'Title','Description'});cards={}
 for r in urows:
  t=key(r['Type'])
  if t in CAT['titles']:r['Title']=CAT['titles'][t];r['Description']=card_text(t,r)
  cards[t]=r
 assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(upgrades,json.dumps(urows,ensure_ascii=False));save(upgrades)
 assert no_text(table('/Game/Data/Upgrades/DT_UpgradeDefinitions')[1],{'Title','Description'})==ub,'Upgrade magnitudes or limits changed'
 tree=unreal.load_asset('/Game/Data/Progression/DA_UpgradeTree');assert tree
 nodes=list(tree.get_editor_property('nodes'));node_guards={}
 guard_fields=['node_id','cost','max_rank','prerequisites','kind','upgrade_type','stat','bonus_per_rank','tree_position','initially_unlocked']
 for index,n in enumerate(nodes):
  node_guards[str(n.get_editor_property('node_id'))]=node_rules(n)
  ident=str(n.get_editor_property('node_id'));t=key(n.get_editor_property('upgrade_type'))
  if ident.startswith('Weapon.') and ident[7:] in CAT['weapons']:
   info=CAT['weapons'][ident[7:]];n.set_editor_property('title',unreal.Text(info['name']))
   n.set_editor_property('description',unreal.Text(info['description']+('\n初始武器，开局自带。' if ident=='Weapon.Projectile' else '\n永久解锁后每局开场自动装备，无需抽取获取卡。' if ident=='Weapon.FrostBlade' else '\n永久解锁后加入本局获取卡池；在战斗中抽到才获得武器。')))
  elif ident.startswith('Card.') and t in cards:
   c=cards[t];n.set_editor_property('title',unreal.Text(c['Title']))
   n.set_editor_property('description',unreal.Text(('初始开放。' if n.get_editor_property('initially_unlocked') else '开放「'+c['Title']+'」升级卡。')+'本局选中后：'+c['Description']))
  # Unreal数组迭代得到结构副本，修改后必须写回元素。
  nodes[index]=n
 for n in nodes:assert node_rules(n)==node_guards[str(n.get_editor_property('node_id'))],'Tree unlock rules changed'
 expected_texts={str(n.get_editor_property('node_id')):(str(n.get_editor_property('title')),str(n.get_editor_property('description'))) for n in nodes}
 tree.set_editor_property('nodes',nodes);save(tree)
 actual_texts={str(n.get_editor_property('node_id')):(str(n.get_editor_property('title')),str(n.get_editor_property('description'))) for n in tree.get_editor_property('nodes')}
 assert actual_texts==expected_texts,'Upgrade tree texts were not written back'
 tutorial=unreal.load_asset('/Game/UMG/Menus/MainMenu/WBP_Tutorial');assert tutorial
 for widget in ['TitleText','TutorialText']:
  text=unreal.RogueWidgetBuilder.get_text_block_text(tutorial,widget)
  assert text,widget
  assert unreal.RogueWidgetBuilder.update_text_block(tutorial,widget,translated(text))
 save(tutorial)
 audio=unreal.load_asset('/Game/Data/Audio/DA_AudioConfig');assert audio
 entries=list(audio.get_editor_property('weapons'));audio_guards=[]
 for e in entries:
  audio_guards.append((str(e.get_editor_property('weapon_class')),e.get_editor_property('attack_audio').export_text()))
  e.set_editor_property('weapon_name',translated(e.get_editor_property('weapon_name')))
 for e,before in zip(entries,audio_guards):assert (str(e.get_editor_property('weapon_class')),e.get_editor_property('attack_audio').export_text())==before
 audio.set_editor_property('weapons',entries);save(audio)
 REPORT['audio_only_labels_changed']=True
 tree_widget=unreal.load_asset('/Game/UMG/Menus/Progression/WBP_UpgradeTree');assert tree_widget
 footer=unreal.RogueWidgetBuilder.get_text_block_text(tree_widget,'FooterText')
 assert footer
 assert unreal.RogueWidgetBuilder.update_text_block(tree_widget,'FooterText',translated(footer))
 save(tree_widget)
 REPORT['weapons']=wrows;REPORT['upgrades']=urows;REPORT['tree_rules_preserved']=True
 if '-ApplyForestAtmosphere' in unreal.SystemLibrary.get_command_line():
  motes=atmosphere_material();sphere=unreal.load_asset('/Engine/BasicShapes/Sphere');assert sphere
  for map_name in ['Battle_Day','Battle_Night']:
   assert LEVEL.load_level('/Game/Map/'+map_name)
   all_actors=ACTORS.get_all_level_actors();night=map_name.endswith('Night');folder='氛围/'+('月夜' if night else '午后')
   def environment(a):return isinstance(a,(unreal.Light,unreal.SkyLight,unreal.ExponentialHeightFog,unreal.PostProcessVolume)) or a.get_actor_label()=='SM_SkySphere'
   guard={a.get_name():protected_actor(a) for a in all_actors if not environment(a)}
   sun=next(a for a in all_actors if isinstance(a,unreal.DirectionalLight));c=sun.get_component_by_class(unreal.DirectionalLightComponent)
   sun.set_actor_label('月夜_斜照月光' if night else '午后_暖色斜阳');sun.set_folder_path(folder)
   sun.set_actor_rotation(unreal.Rotator(pitch=-38 if night else -43,yaw=-35,roll=0),False)
   c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_intensity(.62 if night else 3.3);c.set_light_color(color(.60,.69,1) if night else color(1,.87,.68))
   setp(c,cast_shadows=True,light_source_angle=3.0,atmosphere_sun_light=False,use_temperature=False)
   sky=next(a for a in all_actors if isinstance(a,unreal.SkyLight));c=sky.get_component_by_class(unreal.SkyLightComponent)
   sky.set_actor_label('月夜_青蓝补光' if night else '午后_清冷补光');sky.set_folder_path(folder)
   c.set_intensity(.30 if night else .54);c.set_light_color(color(.42,.57,.84) if night else color(.74,.85,1))
   fog=next(a for a in all_actors if isinstance(a,unreal.ExponentialHeightFog));fog.set_actor_label('月夜_林缘蓝雾' if night else '午后_林间薄雾');fog.set_folder_path(folder)
   fog.set_actor_location(unreal.Vector(0,0,90),False,False)
   setp(fog.get_component_by_class(unreal.ExponentialHeightFogComponent),fog_density=.005 if night else .0022,fog_height_falloff=.24,fog_max_opacity=.32 if night else .20,start_distance=650 if night else 1000,fog_inscattering_luminance=color(.035,.055,.105) if night else color(.32,.39,.46),enable_volumetric_fog=False)
   for a in all_actors:
    if isinstance(a,(unreal.PointLight,unreal.SpotLight)):
     c=a.get_component_by_class(unreal.LightComponent);c.set_visibility(night);c.set_light_color(color(1,.47,.17));c.set_intensity(850 if night else 0);c.set_cast_shadows(False);a.set_folder_path(folder+'/灯笼')
   post=next(a for a in all_actors if isinstance(a,unreal.PostProcessVolume));post.set_folder_path(folder);post.set_actor_label('月夜_氛围色调' if night else '午后_氛围色调');setp(post,enabled=True,unbound=True,blend_weight=1.)
   settings=post.get_editor_property('settings')
   setp(settings,override_auto_exposure_method=True,auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,override_auto_exposure_bias=True,auto_exposure_bias=-.65 if night else -.12,override_auto_exposure_apply_physical_camera_exposure=True,auto_exposure_apply_physical_camera_exposure=False,override_bloom_intensity=True,bloom_intensity=.22 if night else .14,override_bloom_threshold=True,bloom_threshold=1.4,override_vignette_intensity=True,vignette_intensity=.20 if night else .12,override_scene_color_tint=True,scene_color_tint=color(.92,.96,1) if night else color(1,.99,.95),override_color_saturation=True,color_saturation=unreal.Vector4(.93,.93,.93,1),override_color_contrast=True,color_contrast=unreal.Vector4(1.04,1.04,1.04,1),override_ambient_occlusion_intensity=True,ambient_occlusion_intensity=.45,override_ambient_occlusion_radius=True,ambient_occlusion_radius=140.)
   post.set_editor_property('settings',settings)
   # Small, non-interactive, material-driven motes at the forest edge; no actor Tick or lights.
   forest=next(a for a in all_actors if a.get_actor_label()=='Forest_V2_Merged_UE5');center,extent=forest.get_actor_bounds(False)
   rng=random.Random(619 if night else 410);count=36 if night else 24
   for i in range(count):
    angle=(i+rng.random())*2*math.pi/count
    pos=unreal.Vector(center.x+extent.x*.69*math.cos(angle),center.y+extent.y*.66*math.sin(angle),93+rng.uniform(70,240))
    a=ACTORS.spawn_actor_from_class(unreal.StaticMeshActor,pos);assert a
    a.set_actor_label(('月夜_萤火_' if night else '午后_浮尘_')+str(i+1).zfill(2));a.set_folder_path(folder+'/微光');a.set_actor_enable_collision(False)
    c=a.get_component_by_class(unreal.StaticMeshComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(sphere);c.set_material(0,motes['Night' if night else 'Day']);c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);c.set_cast_shadow(False)
    setp(c,can_ever_affect_navigation=False,bounds_scale=16.)
    size=rng.uniform(.025,.045) if night else rng.uniform(.018,.03);a.set_actor_scale3d(unreal.Vector(size,size,size))
   actual={a.get_name():protected_actor(a) for a in ACTORS.get_all_level_actors() if a.get_name() in guard}
   assert actual==guard,'Authored geometry, shop, spawn region or gameplay transform changed'
   assert LEVEL.save_current_level(),map_name
   REPORT['maps'][map_name]={'protected_actors_unchanged':len(guard),'motes':count,'volumetric_fog':False,'lighting_folder':folder}
 REPORT['passed']=True
except Exception:
 REPORT['error']=traceback.format_exc();unreal.log_error(REPORT['error'])
finally:
 (Path(unreal.Paths.project_saved_dir())/'Presentation-text-result.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
 if REPORT['passed']:unreal.log('ATMOSPHERE_AND_NAMES_APPLIED')
