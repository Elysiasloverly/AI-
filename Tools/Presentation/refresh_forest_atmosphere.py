from pathlib import Path
import json,math,random,traceback,unreal
W=Path(__file__).parent;ROOT='/Game/GameAssets/Environment/AtmosphereV2';EDIT=unreal.MaterialEditingLibrary
LEVEL=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ACTORS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
record=Path(unreal.Paths.project_saved_dir())/'ForestAtmosphere-created-assets.json';seed=W/'forest_atmosphere_assets.json';created=json.loads(record.read_text(encoding='utf-8')) if record.exists() else json.loads(seed.read_text(encoding='utf-8'))
result={'passed':False,'maps':{},'created':created}
def setp(o,**kw):
 for k,v in kw.items():o.set_editor_property(k,v)
def col(r,g,b):return unreal.LinearColor(r,g,b,1)
def save(a):assert unreal.RogueEditorAssetUtils.save_asset(a),a.get_path_name()
def remember(path):
 if path not in created:created.append(path)
 record.write_text(json.dumps(created,indent=2),encoding='utf-8')
def node(m,c,x,y):return EDIT.create_material_expression(m,c,x,y)
def wire(a,b,pin='',out=''):assert EDIT.connect_material_expressions(a,out,b,pin),(b.get_name(),pin)
def scalar(m,name,val,x,y):
 n=node(m,unreal.MaterialExpressionScalarParameter,x,y);setp(n,parameter_name=name,default_value=val);return n
def vector(m,name,val,x,y):
 n=node(m,unreal.MaterialExpressionVectorParameter,x,y);setp(n,parameter_name=name,default_value=val);return n
def custom(m,code,inputs,typ,x=0,y=0):
 n=node(m,unreal.MaterialExpressionCustom,x,y);items=[]
 for key in inputs:
  i=unreal.CustomInput();i.set_editor_property('input_name',key);items.append(i)
 setp(n,code=code,output_type=typ,inputs=items)
 for k,v in inputs.items():wire(v,n,k)
 return n
def master(name,domain=unreal.MaterialDomain.MD_SURFACE):
 path=ROOT+'/'+name
 if unreal.EditorAssetLibrary.does_asset_exist(path):
  assert path in created,'Preserve existing user asset: '+path
  m=unreal.load_asset(path);EDIT.delete_all_material_expressions(m)
 else:
  m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ROOT,unreal.Material,unreal.MaterialFactoryNew());assert m;remember(path)
 setp(m,material_domain=domain)
 return m
def instance(name,parent,params):
 path=ROOT+'/'+name
 if unreal.EditorAssetLibrary.does_asset_exist(path):assert path in created;m=unreal.load_asset(path)
 else:
  m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ROOT,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew());assert m;remember(path)
 EDIT.set_material_instance_parent(m,parent)
 for k,v in params.items():
  if isinstance(v,unreal.LinearColor):EDIT.set_material_instance_vector_parameter_value(m,k,v)
  else:EDIT.set_material_instance_scalar_parameter_value(m,k,v)
 EDIT.update_material_instance(m);save(m);return m
noise=r'''
struct Noise2 {
 float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
 float value(float2 p) {
  float2 i=floor(p),f=frac(p); f=f*f*(3-2*f);
  return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+float2(1,1)),f.x),f.y);
 }
}; Noise2 n;
'''
try:
 # A genuine projected light function makes the broad clearing read as a forest floor.
 m=master('M_CanopyLight',unreal.MaterialDomain.MD_LIGHT_FUNCTION)
 p=node(m,unreal.MaterialExpressionWorldPosition,-900,-200);t=node(m,unreal.MaterialExpressionTime,-900,0)
 floor=scalar(m,'ShadowFloor',.25,-900,180);scale=scalar(m,'PatternScale',.0012,-900,360)
 d=custom(m,noise+r'''
 float2 q=P.xy*Scale+float2(T*.012,T*.008);
 float v=.55*n.value(q)+.3*n.value(q*2.1+4)+.15*n.value(q*4.5+9);
 float mask=lerp(Floor,1,smoothstep(.31,.68,v));
 return float3(mask,mask,mask);
 ''',{'P':p,'T':t,'Floor':floor,'Scale':scale},unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 assert EDIT.connect_material_property(d,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);EDIT.recompile_material(m);save(m)
 canopy={}
 for name,f in [('Day',.30),('Night',.48)]:canopy[name]=instance('MI_Canopy_'+name,m,{'ShadowFloor':f,'PatternScale':.0011})
 # Soft, drifting wisps occupy the inner forest edge and a few thin lanes across the clearing.
 m=master('M_GroundMist');setp(m,blend_mode=unreal.BlendMode.BLEND_TRANSLUCENT,shading_model=unreal.MaterialShadingModel.MSM_UNLIT,two_sided=True,use_translucency_vertex_fog=False)
 uv=node(m,unreal.MaterialExpressionTextureCoordinate,-1000,-300);t=node(m,unreal.MaterialExpressionTime,-1000,-100)
 tint=vector(m,'Tint',col(.35,.43,.52),-1000,100);opacity=scalar(m,'Opacity',.12,-1000,300)
 d=custom(m,noise+r'''
 float2 p=(UV-.5)*2;
 float edge=pow(saturate(1-dot(p,p)),2.2);
 float2 q=UV*float2(4.5,2)+float2(-T*.018,T*.006);
 float v=.7*n.value(q)+.3*n.value(q*2.2+8);
 float ribbons=smoothstep(.17,.76,v);
 return float4(Tint.rgb,saturate(edge*ribbons*Opacity));
 ''',{'UV':uv,'T':t,'Tint':tint,'Opacity':opacity},unreal.CustomMaterialOutputType.CMOT_FLOAT4)
 rgb=node(m,unreal.MaterialExpressionComponentMask,220,-120);setp(rgb,r=True,g=True,b=True,a=False);wire(d,rgb)
 alpha=node(m,unreal.MaterialExpressionComponentMask,220,100);setp(alpha,r=False,g=False,b=False,a=True);wire(d,alpha)
 fade=node(m,unreal.MaterialExpressionDepthFade,460,100);setp(fade,fade_distance_default=65.);wire(alpha,fade,'Opacity')
 assert EDIT.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);assert EDIT.connect_material_property(fade,'',unreal.MaterialProperty.MP_OPACITY)
 EDIT.recompile_material(m);save(m)
 mist={}
 for name,tint,opacity in [('Day',col(.28,.34,.31),.045),('Night',col(.045,.075,.12),.08)]:mist[name]=instance('MI_GroundMist_'+name,m,{'Tint':tint,'Opacity':opacity})
 # Small amber fireflies pulse asynchronously, distinct from the solid hostile bullets.
 m=master('M_Firefly');setp(m,blend_mode=unreal.BlendMode.BLEND_ADDITIVE,shading_model=unreal.MaterialShadingModel.MSM_UNLIT,two_sided=False)
 t=node(m,unreal.MaterialExpressionTime,-1000,-250);p=node(m,unreal.MaterialExpressionWorldPosition,-1000,-80)
 n=node(m,unreal.MaterialExpressionPixelNormalWS,-1000,80);v=node(m,unreal.MaterialExpressionCameraVectorWS,-1000,220)
 tint=vector(m,'Tint',col(1,.70,.23),-1000,380);energy=scalar(m,'Energy',5.,-1000,540)
 d=custom(m,r'''
 float phase=P.x*.0013+P.y*.0021;
 float pulse=.12+.88*pow(.5+.5*sin(T*1.15+phase),3);
 float edge=pow(saturate(dot(normalize(N),normalize(V))),2);
 return float4(Tint.rgb*Energy,saturate(edge*pulse*.65));
 ''',{'T':t,'P':p,'N':n,'V':v,'Tint':tint,'Energy':energy},unreal.CustomMaterialOutputType.CMOT_FLOAT4)
 for channel,prop in [('rgb',unreal.MaterialProperty.MP_EMISSIVE_COLOR),('a',unreal.MaterialProperty.MP_OPACITY)]:
  mask=node(m,unreal.MaterialExpressionComponentMask,220,0 if channel=='rgb' else 180);setp(mask,r=channel=='rgb',g=channel=='rgb',b=channel=='rgb',a=channel=='a');wire(d,mask);assert EDIT.connect_material_property(mask,'',prop)
 drift=custom(m,'float phase=P.x*.002+P.y*.001;return float3(22*sin(T*.31+phase),16*cos(T*.25+phase),20*sin(T*.43+phase));',{'T':t,'P':p},unreal.CustomMaterialOutputType.CMOT_FLOAT3,0,500)
 assert unreal.RogueEditorAssetUtils.connect_world_position_offset(m,drift);EDIT.recompile_material(m);save(m)
 firefly={}
 for name,tint,energy in [('Day',col(1,.75,.33),1.2),('Night',col(1,.68,.20),5.)]:firefly[name]=instance('MI_Firefly_'+name,m,{'Tint':tint,'Energy':energy})
 plane=unreal.load_asset('/Engine/BasicShapes/Plane');assert plane
 def protected(a):
  d={'class':a.get_class().get_path_name(),'transform':a.get_actor_transform().export_text(),'meshes':[]}
  for c in a.get_components_by_class(unreal.StaticMeshComponent):
   mesh=c.get_editor_property('static_mesh');d['meshes'].append((c.get_name(),mesh.get_path_name() if mesh else None,c.get_relative_transform().export_text(),[x.get_path_name() if x else None for x in c.get_materials()],str(c.get_collision_enabled())))
  return d
 for mapname in ['Battle_Day','Battle_Night']:
  assert LEVEL.load_level('/Game/Map/'+mapname);night=mapname.endswith('Night');key='Night' if night else 'Day';allactors=ACTORS.get_all_level_actors()
  def managed(a):return isinstance(a,(unreal.Light,unreal.SkyLight,unreal.ExponentialHeightFog,unreal.PostProcessVolume)) or a.get_actor_label().startswith(('月夜_萤火_','午后_浮尘_','氛围V2_')) or a.get_actor_label()=='SM_SkySphere'
  guard={a.get_name():protected(a) for a in allactors if not managed(a)}
  sun=next(a for a in allactors if isinstance(a,unreal.DirectionalLight));c=sun.get_component_by_class(unreal.DirectionalLightComponent)
  c.set_intensity(.38 if night else 4.3);c.set_light_color(col(.60,.67,.90) if night else col(1,.78,.47));c.set_cast_shadows(True);c.set_light_function_material(canopy[key]);c.set_volumetric_scattering_intensity(1.0 if night else 1.4)
  setp(c,light_source_angle=2.,light_function_fade_distance=25000.,disabled_brightness=1.)
  sun.set_actor_rotation(unreal.Rotator(pitch=-36 if night else -32,yaw=-38,roll=0),False)
  sky=next(a for a in allactors if isinstance(a,unreal.SkyLight));c=sky.get_component_by_class(unreal.SkyLightComponent);c.set_intensity(.17 if night else .24);c.set_light_color(col(.54,.60,.85) if night else col(.67,.78,1))
  spots=[]
  for a in allactors:
   if isinstance(a,unreal.SpotLight):
    c=a.get_component_by_class(unreal.SpotLightComponent);spots.append({'name':a.get_name(),'radius':c.get_editor_property('attenuation_radius'),'before_intensity':c.get_editor_property('intensity')});c.set_intensity(0);c.set_visibility(False)
   elif isinstance(a,unreal.PointLight):
    c=a.get_component_by_class(unreal.PointLightComponent);c.set_visibility(night);c.set_intensity(780 if night else 0);c.set_light_color(col(1,.37,.10));c.set_volumetric_scattering_intensity(.20 if night else 0)
  fog=next(a for a in allactors if isinstance(a,unreal.ExponentialHeightFog));c=fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
  fog.set_actor_location(unreal.Vector(0,0,100),False,False)
  setp(c,fog_density=.012 if night else .006,fog_height_falloff=.22,fog_max_opacity=.55 if night else .35,start_distance=350 if night else 600,fog_inscattering_luminance=col(.025,.040,.085) if night else col(.27,.30,.30),enable_volumetric_fog=True,volumetric_fog_scattering_distribution=.55,volumetric_fog_albedo=unreal.Color(220,230,245,255),volumetric_fog_extinction_scale=1.0,volumetric_fog_distance=9500.,volumetric_fog_start_distance=180.,volumetric_fog_emissive=col(.002,.004,.009) if night else col(0,0,0))
  post=next(a for a in allactors if isinstance(a,unreal.PostProcessVolume));setp(post,enabled=True,unbound=True,blend_weight=1.,priority=10.)
  s=post.get_editor_property('settings')
  setp(s,override_auto_exposure_bias=True,auto_exposure_bias=-.75 if night else -.25,override_scene_color_tint=True,scene_color_tint=col(.84,.88,1.) if night else col(1.,.96,.86),override_color_gain=True,color_gain=unreal.Vector4(.76,.79,.88,1) if night else unreal.Vector4(1.,.97,.90,1),override_color_gain_shadows=True,color_gain_shadows=unreal.Vector4(.82,.86,1.0,1) if night else unreal.Vector4(.78,.89,1.02,1),override_color_saturation=True,color_saturation=unreal.Vector4(.83,.83,.83,1) if night else unreal.Vector4(.88,.88,.88,1),override_color_contrast=True,color_contrast=unreal.Vector4(1.12,1.12,1.12,1) if night else unreal.Vector4(1.13,1.13,1.13,1),override_color_gamma_shadows=True,color_gamma_shadows=unreal.Vector4(1,1,1,1.10 if night else 1),override_bloom_intensity=True,bloom_intensity=.38 if night else .21,override_bloom_threshold=True,bloom_threshold=1.3,override_vignette_intensity=True,vignette_intensity=.28 if night else .18,override_ambient_occlusion_intensity=True,ambient_occlusion_intensity=.65)
  post.set_editor_property('settings',s)
  # Existing light points were outside the playable clearing, often behind the trees.
  motes=[a for a in allactors if a.get_actor_label().startswith(('月夜_萤火_','午后_浮尘_'))];rng=random.Random(520 if night else 340)
  for i,a in enumerate(motes):
   angle=(i+rng.random())*2*math.pi/len(motes);radius=rng.uniform(.82,1.08)
   a.set_actor_location(unreal.Vector(120+3400*radius*math.cos(angle),-10+2700*radius*math.sin(angle),rng.uniform(170,330)),False,False)
   size=rng.uniform(.055,.09) if night else rng.uniform(.025,.045);a.set_actor_scale3d(unreal.Vector(size,size,size));c=a.get_component_by_class(unreal.StaticMeshComponent);c.set_material(0,firefly[key]);setp(c,bounds_scale=18.)
  mist_positions=[(-2300,900,165,36,12,-18),(2200,-1000,150,34,11,24),(0,2600,180,46,13,5),(-900,-2400,145,42,10,-8),(500,800,155,32,7,22)]
  for i,(x,y,z,sx,sy,yaw) in enumerate(mist_positions):
   label='氛围V2_贴地流雾_'+str(i+1)
   a=next((a for a in allactors if a.get_actor_label()==label),None)
   if not a:a=ACTORS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,y,z));assert a
   a.set_actor_label(label);a.set_folder_path('氛围/'+('月夜' if night else '午后')+'/贴地流雾');a.set_actor_enable_collision(False);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False);a.set_actor_scale3d(unreal.Vector(sx,sy,1))
   c=a.get_component_by_class(unreal.StaticMeshComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(plane);c.set_material(0,mist[key]);c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);c.set_cast_shadow(False);setp(c,can_ever_affect_navigation=False,translucency_sort_priority=-15)
  assert {a.get_name():protected(a) for a in ACTORS.get_all_level_actors() if a.get_name() in guard}==guard,'Authored geometry, shop or gameplay changed'
  assert LEVEL.save_current_level()
  result['maps'][mapname]={'protected_actors_unchanged':len(guard),'relocated_motes':len(motes),'mist_planes':5,'volumetric_fog':True,'disabled_broad_spot_lights':spots}
 result['passed']=True
except Exception:
 result['error']=traceback.format_exc();unreal.log_error(result['error'])
finally:
 (Path(unreal.Paths.project_saved_dir())/'ForestAtmosphere-result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
 if result['passed']:unreal.log('FOREST_ATMOSPHERE_V2_SAVED')
