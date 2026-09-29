import unreal
ROOT='/Game/Combat/Readability'
tools=unreal.AssetToolsHelpers.get_asset_tools();e=unreal.MaterialEditingLibrary
task=unreal.AssetImportTask()
task.set_editor_property('filename',unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()+'SourceArt/Combat/T_GroundBurstSigil.png'))
task.set_editor_property('destination_path',ROOT)
task.set_editor_property('automated',True)
task.set_editor_property('replace_existing',True)
task.set_editor_property('save',True)
tools.import_asset_tasks([task])
sigil=unreal.load_asset(ROOT+'/T_GroundBurstSigil')
assert sigil, 'Missing authored warning texture'
assert unreal.EditorAssetLibrary.save_loaded_asset(sigil, only_if_is_dirty=False), 'Warning texture save failed; close any editor holding this package before retrying'

def param(m,name,value):
    n=e.create_material_expression(m,unreal.MaterialExpressionScalarParameter)
    n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n

def create(name,decal,code):
    m=unreal.load_asset(ROOT+'/'+name) or tools.create_asset(name,ROOT,unreal.Material,unreal.MaterialFactoryNew())
    e.delete_all_material_expressions(m)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('two_sided',True)
    if decal:m.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL)
    else:m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    uv=e.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
    n=e.create_material_expression(m,unreal.MaterialExpressionCustom)
    n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    n.set_editor_property('code',code)
    inputs=[]
    for key in ['UV','Progress','Shape','Burst']:
        item=unreal.CustomInput();item.set_editor_property('input_name',key);inputs.append(item)
    n.set_editor_property('inputs',inputs);e.connect_material_expressions(uv,'',n,'UV')
    for key in ['Progress','Shape','Burst']:e.connect_material_expressions(param(m,key,0.),'',n,key)
    color=e.create_material_expression(m,unreal.MaterialExpressionVectorParameter)
    color.set_editor_property('parameter_name','CueColor');color.set_editor_property('default_value',unreal.LinearColor(1,.13,.025,1) if decal else unreal.LinearColor(.8,.9,1,1))
    if not decal:
        tex=e.create_material_expression(m,unreal.MaterialExpressionTextureSampleParameter2D)
        tex.set_editor_property('parameter_name','Icon');tex.set_editor_property('texture',unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
        mix=e.create_material_expression(m,unreal.MaterialExpressionLinearInterpolate)
        e.connect_material_expressions(n,'',mix,'A');e.connect_material_expressions(tex,'A',mix,'B');e.connect_material_expressions(param(m,'UseIcon',0.),'',mix,'Alpha')
        opacity=mix
    else:
        tex=e.create_material_expression(m,unreal.MaterialExpressionTextureSampleParameter2D)
        tex.set_editor_property('parameter_name','Sigil')
        tex.set_editor_property('texture',sigil)
        # Keep the mathematical radius readable; painted texture only adds detail.
        mix=e.create_material_expression(m,unreal.MaterialExpressionMax)
        gain=e.create_material_expression(m,unreal.MaterialExpressionMultiply)
        e.connect_material_expressions(tex,'A',gain,'A')
        e.connect_material_expressions(param(m,'SigilOpacity',.65),'',gain,'B')
        e.connect_material_expressions(n,'',mix,'A');e.connect_material_expressions(gain,'',mix,'B')
        opacity=mix
        painted=e.create_material_expression(m,unreal.MaterialExpressionMultiply)
        e.connect_material_expressions(tex,'RGB',painted,'A')
        e.connect_material_expressions(param(m,'SigilEmission',1.6),'',painted,'B')
        emission=e.create_material_expression(m,unreal.MaterialExpressionAdd)
        e.connect_material_expressions(painted,'',emission,'A')
        tint=e.create_material_expression(m,unreal.MaterialExpressionMultiply)
        e.connect_material_expressions(color,'',tint,'A');e.connect_material_expressions(n,'',tint,'B')
        e.connect_material_expressions(tint,'',emission,'B')
        color=emission
        body=e.create_material_expression(m,unreal.MaterialExpressionVectorParameter)
        body.set_editor_property('parameter_name','SigilBodyColor')
        body.set_editor_property('default_value',unreal.LinearColor(.12,.008,.003,1))
        e.connect_material_property(body,'',unreal.MaterialProperty.MP_BASE_COLOR)
        e.connect_material_property(param(m,'GroundRoughness',1.),'',unreal.MaterialProperty.MP_ROUGHNESS)
    fade=e.create_material_expression(m,unreal.MaterialExpressionMultiply)
    e.connect_material_expressions(opacity,'',fade,'A')
    e.connect_material_expressions(param(m,'RenderOpacity',1.),'',fade,'B')
    e.connect_material_property(fade,'',unreal.MaterialProperty.MP_OPACITY)
    e.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    e.recompile_material(m)
    assert unreal.EditorAssetLibrary.save_loaded_asset(m), 'Material save failed: '+name

create('M_AttackRecognition',False,r'''
float2 p=(UV-.5)*2;float r=length(p);float a=atan2(p.y,p.x);
float outline=1-smoothstep(.025,.055,abs(r-.72));
float mask=0;
if(Shape<1.5){float d=min(abs(p.x),abs(p.y));mask=(1-smoothstep(.025,.07,d))*(1-smoothstep(.52,.74,r));}
else if(Shape<2.5){mask=outline*step(.05,p.y);float2 q=abs(p);mask=max(mask,(1-smoothstep(.035,.075,abs(q.x-.65+q.y*.6)))*step(q.y,.3));}
else if(Shape<3.5){mask=(1-smoothstep(.035,.085,min(abs(p.y-p.x),abs(p.y+p.x))))*(1-smoothstep(.6,.8,r));}
else if(Shape<4.5){mask=outline*(.3+.7*step(frac((a+3.14159)/6.28318*8),.65));mask=max(mask,(1-smoothstep(.025,.06,abs(r-(.25+.35*(1-Progress)))))*.65);}
else{mask=outline;mask=max(mask,(1-smoothstep(.035,.07,abs(p.x)))*step(abs(p.y),.38));}
return saturate(mask)*(.65+.35*Progress);
''')
create('M_GroundBurstWarning',True,r'''
float2 p=(UV-.5)*2;float r=length(p);float a=atan2(p.y,p.x);
float edge=1-smoothstep(.008,.022,abs(r-.985));
float dash=step(.35,frac((a+3.14159)/6.28318*32));
float inner=(1-smoothstep(.009,.025,abs(r-.89)))*dash;
float sweep=(1-smoothstep(.008,.028,abs(r-(.1+.78*(1-Progress)))))*.4;
float fill=(1-smoothstep(.96,1,r))*(.035+.08*Progress+Burst*.4);
float shock=(1-smoothstep(.015,.07,abs(r-(.12+.86*(1-Burst)))))*step(.001,Burst);
return saturate(edge*(.55+.4*Progress)+inner*.35+sweep+fill+shock)*step(r,1.);
''')
unreal.log_warning('ATTACK_READABILITY_MATERIALS_SAVED')

# A thin colored rim on the weapon itself supplements shape/pose recognition.
m=unreal.load_asset(ROOT+'/M_BladeRecognition') or tools.create_asset('M_BladeRecognition',ROOT,unreal.Material,unreal.MaterialFactoryNew())
e.delete_all_material_expressions(m)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided',True)
e.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
rim=e.create_material_expression(m,unreal.MaterialExpressionFresnel)
rim.set_editor_property('exponent',2.)
rim.set_editor_property('base_reflect_fraction',.35)
strength=param(m,'CueStrength',0.)
opacity=e.create_material_expression(m,unreal.MaterialExpressionMultiply)
e.connect_material_expressions(rim,'',opacity,'A');e.connect_material_expressions(strength,'',opacity,'B')
e.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
color=e.create_material_expression(m,unreal.MaterialExpressionVectorParameter)
color.set_editor_property('parameter_name','CueColor');color.set_editor_property('default_value',unreal.LinearColor(.1,.8,1,1))
glow=e.create_material_expression(m,unreal.MaterialExpressionMultiply)
e.connect_material_expressions(color,'',glow,'A');e.connect_material_expressions(param(m,'Brightness',4.),'',glow,'B')
e.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
e.recompile_material(m)
assert unreal.EditorAssetLibrary.save_loaded_asset(m), 'Blade recognition material save failed'
unreal.log_warning('BLADE_RECOGNITION_MATERIAL_SAVED')

# Keep the refined blade cue when regenerating the full readability asset set.
import runpy
runpy.run_path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()+'Scripts/refine_blade_readability.py'))
