import unreal,math,shutil,datetime
from pathlib import Path
root=Path('D:/Udev/HearBlade');src=root/'SourceArt/Environment/CourtResonators';dst='/Game/Environment/CourtResonators'
backup=root/'Saved/Backups'/('CourtRhythmSky_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
shutil.copytree(root/'Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson',backup/'ExternalActors')
names=['SM_Court_MetronomeBase','SM_Court_TuningRing','SM_Court_Pendulum','SM_Court_ResonanceDisc'];tasks=[]
for name in names:
    t=unreal.AssetImportTask();t.filename=str(src/(name+'.fbx'));t.destination_path=dst+'/Meshes';t.automated=True;t.replace_existing=True;t.save=True
    o=unreal.FbxImportUI();o.import_mesh=True;o.import_materials=False;o.import_textures=False;o.import_as_skeletal=False;o.set_editor_property('automated_import_should_detect_type',False);o.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;o.static_mesh_import_data.combine_meshes=True;o.static_mesh_import_data.auto_generate_collision=False;t.options=o;tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
meshes={}
for name in names:
    m=unreal.load_asset(dst+'/Meshes/'+name);m.modify()
    for i,s in enumerate(m.static_materials):m.set_material(i,unreal.load_asset('/Game/Environment/HarmonicCourtV2/Materials/M_'+str(s.material_slot_name)))
    unreal.EditorAssetLibrary.save_loaded_asset(m,False);meshes[name]=m
# Analytic sky with layered clouds and a restrained gilded horizon.
path=dst+'/Materials/M_Court_TwilightSky'
m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_Court_TwilightSky',dst+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
m.set_editor_property('two_sided',True);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('is_sky',True)
e=unreal.MaterialEditingLibrary;e.delete_all_material_expressions(m)
custom=e.create_material_expression(m,unreal.MaterialExpressionCustom);custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
custom.set_editor_property('code','''
struct SkyNoise {
 float hash(float3 p) { p=frac(p*.1031);p+=dot(p,p.yzx+33.33);return frac((p.x+p.y)*p.z); }
 float noise(float3 p) {float3 i=floor(p),f=frac(p);f=f*f*(3-2*f);
 return lerp(lerp(lerp(hash(i),hash(i+float3(1,0,0)),f.x),lerp(hash(i+float3(0,1,0)),hash(i+float3(1,1,0)),f.x),f.y),lerp(lerp(hash(i+float3(0,0,1)),hash(i+float3(1,0,1)),f.x),lerp(hash(i+float3(0,1,1)),hash(i+float3(1,1,1)),f.x),f.y),f.z);}
};
SkyNoise N;
float3 d=normalize(Direction);
float h=saturate(d.z);
float3 zenith=float3(.07,.11,.25);
float3 horizon=float3(.28,.215,.22);
float3 col=lerp(horizon,zenith,smoothstep(-.02,.78,h));
float3 p=d*5+float3(MusicPhase,0,MusicPhase*.3);
float cloud=N.noise(p)*.55+N.noise(p*2.07)*.28+N.noise(p*4.13)*.17;
cloud=smoothstep(.46,.76,cloud)*smoothstep(.015,.20,h);
col=lerp(col,float3(.24,.245,.30),cloud*.72);
float sun=saturate(dot(d,normalize(float3(.7,-.25,.35))));
col+=float3(.9,.57,.28)*(pow(sun,1800)*.65+pow(sun,70)*.055);
return col*(1+saturate(BeatLift));
''')
inputs=[]
for n in ['Direction','BeatLift','MusicPhase']:
    item=unreal.CustomInput();item.set_editor_property('input_name',n);inputs.append(item)
custom.set_editor_property('inputs',inputs)
pos=e.create_material_expression(m,unreal.MaterialExpressionWorldPosition);cam=e.create_material_expression(m,unreal.MaterialExpressionCameraPositionWS);subtract=e.create_material_expression(m,unreal.MaterialExpressionSubtract)
e.connect_material_expressions(pos,'',subtract,'A');e.connect_material_expressions(cam,'',subtract,'B');e.connect_material_expressions(subtract,'',custom,'Direction')
for name,value in [('BeatLift',.0125),('MusicPhase',0.)]:
    node=e.create_material_expression(m,unreal.MaterialExpressionScalarParameter);node.set_editor_property('parameter_name',name);node.set_editor_property('default_value',value);e.connect_material_expressions(node,'',custom,name)
e.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);e.recompile_material(m)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/ThirdPerson/Lvl_ThirdPerson');actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
existing={a.get_actor_label():a for a in actors.get_all_level_actors()}
for i in range(12):
    angle=2*math.pi*i/12+.12;x=16.0*math.cos(angle);y=16.0*math.sin(angle)
    for name,mode,z in [('SM_Court_MetronomeBase','Static',0),('SM_Court_TuningRing','BeatGimbal',1.25),('SM_Court_Pendulum','BeatPendulum',1.49),('SM_Court_ResonanceDisc','BeatDisc',0)]:
        label='CourtRhythm_'+mode+'_'+str(i)
        # Discs sit nearer the arena edge, bases stay outside the central fight area.
        factor=1.08 if mode=='BeatDisc' else 1.0
        a=existing.get(label) or actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(500+y*100*factor,220+x*100*factor,z*100))
        a.modify();a.set_actor_label(label);a.set_actor_rotation(unreal.Rotator(yaw=180-math.degrees(angle)),False)
        a.tags=[unreal.Name('HB_HarmonicCourt'),unreal.Name(mode),unreal.Name('HB_CourtResonators')]
        c=a.static_mesh_component;c.modify();c.set_static_mesh(meshes[name]);c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        if mode!='Static':c.set_mobility(unreal.ComponentMobility.MOVABLE)
sky_found=False
for a in actors.get_all_level_actors():
    if isinstance(a,unreal.StaticMeshActor):
        sm=a.static_mesh_component.static_mesh
        if sm and sm.get_path_name().startswith('/Engine/EngineSky/SM_SkySphere'):
            a.modify();a.static_mesh_component.modify();a.static_mesh_component.set_material(0,m)
            a.tags=list(set([*a.tags,unreal.Name('HB_RhythmSky')]))
            a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);sky_found=True
    if a.get_class().get_name() in ['VolumetricCloud','SkyAtmosphere']:
        a.modify();a.set_actor_hidden_in_game(True)
        for comp in a.get_components_by_class(unreal.SceneComponent):comp.modify();comp.set_visibility(False,True)
    if a.get_class().get_name()=='SkyLight':
        comp=a.get_component_by_class(unreal.SkyLightComponent);comp.modify();comp.set_editor_property('real_time_capture',False)
    if a.get_class().get_name()=='ExponentialHeightFog':
        comp=a.get_component_by_class(unreal.ExponentialHeightFogComponent);comp.modify()
        comp.set_fog_density(.002)
        comp.set_fog_inscattering_color(unreal.LinearColor(.28,.24,.30,1))
if not sky_found:raise RuntimeError('No sky sphere found')
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
unreal.log_warning('COURT_RHYTHM_SKY_INSTALLED 48 new objects; sky MID driven by music clock')


