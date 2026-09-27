"""Unreal commandlet import/assembly; model geometry is authored in Blender MCP."""
import unreal,json,os,shutil,datetime
from pathlib import Path
ROOT=Path('D:/Udev/HearBlade');SOURCE=ROOT/'SourceArt/Environment/HarmonicCourtV2';DEST='/Game/Environment/HarmonicCourtV2'
backup=ROOT/'Saved/Backups'/('CourtV2_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
for relative in ['Content/ThirdPerson/Lvl_ThirdPerson.umap','Content/__ExternalActors__/ThirdPerson','Content/__ExternalObjects__/ThirdPerson']:
    src=ROOT/relative;dst=backup/relative;dst.parent.mkdir(parents=True,exist_ok=True)
    if src.is_dir():shutil.copytree(src,dst)
    elif src.is_file():shutil.copy2(src,dst)
rows=json.loads((SOURCE/'manifest.json').read_text());asset_tools=unreal.AssetToolsHelpers.get_asset_tools();tasks=[]
for row in rows:
    t=unreal.AssetImportTask();t.filename=str(SOURCE/(row['name']+'.fbx'));t.destination_path=DEST+'/Meshes';t.automated=True;t.replace_existing=True;t.save=True
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False;opt.set_editor_property('automated_import_should_detect_type',False);opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=False;t.options=opt;tasks.append(t)
    if row['mode']=='Static':opt.static_mesh_import_data.auto_generate_collision=True
for file in SOURCE.glob('T_*.png'):
    t=unreal.AssetImportTask();t.filename=str(file);t.destination_path=DEST+'/Textures';t.automated=True;t.replace_existing=True;t.save=True;tasks.append(t)
asset_tools.import_asset_tasks(tasks)
pal={'HC_Ivory':((.43,.44,.415),0,.52,0),'HC_Gold':((.48,.33,.15),.8,.30,0),'HC_Blue':((.025,.047,.078),.35,.30,0),'HC_Cyan':((.045,.25,.33),.25,.32,.7),'HC_Speaker':((.008,.019,.026),0,.55,0),'HC_Stone':((.26,.285,.31),0,.8,0),'HC_Foliage':((.045,.085,.055),0,.8,0),'HC_Flower':((.16,.055,.12),0,.7,0)}
edit=unreal.MaterialEditingLibrary;mats={}
for name,(color,metal,rough,glow) in pal.items():
    m=asset_tools.create_asset('M_'+name,DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew()) if not unreal.EditorAssetLibrary.does_asset_exist(DEST+'/Materials/M_'+name) else unreal.load_asset(DEST+'/Materials/M_'+name)
    edit.delete_all_material_expressions(m)
    texture_name='T_HC_Limestone_Detail' if name=='HC_Ivory' else 'T_'+name+'_Color'
    tex=unreal.load_asset(DEST+'/Textures/'+texture_name) if name in ['HC_Ivory','HC_Blue','HC_Gold'] else None
    if tex:
        tex.set_editor_property('address_x',unreal.TextureAddress.TA_MIRROR);tex.set_editor_property('address_y',unreal.TextureAddress.TA_MIRROR)
        obj=edit.create_material_expression(m,unreal.MaterialExpressionTextureObject);obj.texture=tex
        fn=edit.create_material_expression(m,unreal.MaterialExpressionMaterialFunctionCall);fn.set_material_function(unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture'))
        inputs=edit.get_material_expression_input_names(fn);unreal.log_warning('WORLD_TEXTURE_INPUTS '+str(inputs))
        pin=next(x for x in inputs if x.startswith('TextureObject'));assert edit.connect_material_expressions(obj,'',fn,pin)
        size=edit.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);size.constant=unreal.LinearColor(200,200,200,1)
        pin=next(x for x in inputs if x.startswith('TextureSize'));assert edit.connect_material_expressions(size,'',fn,pin)
        linked=False
        for output in ['XYZ Texture','XYZTexture']:
            if edit.connect_material_property(fn,output,unreal.MaterialProperty.MP_BASE_COLOR):linked=True;break
        if not linked:raise RuntimeError('WorldAlignedTexture output unavailable')
    else:
        c=edit.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);c.constant=unreal.LinearColor(*color,1);edit.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
    for value,prop in [(metal,unreal.MaterialProperty.MP_METALLIC),(rough,unreal.MaterialProperty.MP_ROUGHNESS)]:
        v=edit.create_material_expression(m,unreal.MaterialExpressionConstant);v.r=value;edit.connect_material_property(v,'',prop)
    if glow:
        c=edit.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);c.constant=unreal.LinearColor(*(v*glow for v in color),1);edit.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.recompile_material(m);mats[name]=m
meshes={};sm_editor=unreal.new_object(unreal.StaticMeshEditorSubsystem)
for row in rows:
    mesh=unreal.load_asset(DEST+'/Meshes/'+row['name']);assert isinstance(mesh,unreal.StaticMesh)
    mesh.modify()
    for i,slot in enumerate(mesh.static_materials):
        name=str(slot.material_slot_name)
        if name in mats:mesh.set_material(i,mats[name])
        else:raise RuntimeError('Unmapped material '+name)
    if row['mode']=='Static':
        mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
        for i in range(mesh.get_num_sections(0)):sm_editor.enable_section_collision(mesh,True,0,i)
        count=sm_editor.get_convex_collision_count(mesh)
        if count<33:raise RuntimeError('Missing authored floor / wall collision: '+str(count))
        unreal.log_warning('COURT_AUTHORED_COLLIDERS '+str(count))
    unreal.EditorAssetLibrary.save_loaded_asset(mesh,False);meshes[row['name']]=mesh
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/ThirdPerson/Lvl_ThirdPerson')
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);deletions=[]
for a in sub.get_all_level_actors():
    tags=[str(t) for t in a.tags];remove='HB_HarmonicCourt' in tags or 'HB_ArchivedPrototype' in tags
    if isinstance(a,unreal.StaticMeshActor):
        mesh=a.static_mesh_component.static_mesh;path=mesh.get_path_name() if mesh else ''
        remove|=path.startswith('/Game/LevelPrototyping/') or path.startswith('/Engine/MapTemplates/SM_Template_Map_Floor')
    if remove:
        package=a.get_package().get_name();assert package.startswith('/Game/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/'),package
        deletions.append({'label':a.get_actor_label(),'package':package});a.modify();sub.destroy_actor(a)
for row in rows:
    x,y,z=row['location'];a=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(500+y*100,220+x*100,z*100),unreal.Rotator(pitch=0,yaw=90,roll=0))
    a.modify();a.set_actor_label('HC2_'+row['name']);a.tags=[unreal.Name('HB_HarmonicCourt'),unreal.Name(row['mode'])]
    c=a.static_mesh_component;c.modify();c.set_static_mesh(meshes[row['name']])
    if row['mode']=='Static':c.set_collision_profile_name('BlockAll')
    else:c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
controller=sub.spawn_actor_from_class(unreal.load_class(None,'/Script/HearBlade.SoundstageBeatController'),unreal.Vector(500,220,0));controller.set_actor_label('Harmonic Court - Beat Motion');controller.tags=[unreal.Name('HB_HarmonicCourt')]
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
# Deleted external Actor packages are empty in memory but commandlet save can leave
# their old files behind. Remove only packages belonging to actors selected above.
safe_root=(ROOT/'Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson').resolve()
for record in deletions:
    file=(ROOT/'Content'/(record['package'][6:]+'.uasset')).resolve()
    if not file.is_relative_to(safe_root):raise RuntimeError('Deletion escaped map: '+str(file))
    relative=file.relative_to(ROOT);copy=backup/relative
    if not copy.is_file():raise RuntimeError('Missing deletion backup '+str(copy))
    if file.is_file():file.unlink()
(backup/'removed_actors.json').write_text(json.dumps(deletions,ensure_ascii=False,indent=2),encoding='utf8')
unreal.log_warning('COURT_V2_SUCCESS meshes='+str(len(rows))+' removed='+str(len(deletions))+' backup='+str(backup))
