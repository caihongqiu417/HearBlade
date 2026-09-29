"""Original procedural blade cue: tapered brush cuts, hot core and ink silhouette."""
import unreal
e=unreal.MaterialEditingLibrary
m=unreal.load_asset('/Game/Combat/Readability/M_AttackRecognition')
assert m
e.delete_all_material_expressions(m)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided',True)
def scalar(name,value):
    n=e.create_material_expression(m,unreal.MaterialExpressionScalarParameter)
    n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n
uv=e.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
color=e.create_material_expression(m,unreal.MaterialExpressionVectorParameter)
color.set_editor_property('parameter_name','CueColor');color.set_editor_property('default_value',unreal.LinearColor(.8,.9,1,1))
n=e.create_material_expression(m,unreal.MaterialExpressionCustom)
n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
inputs=[]
for name in ['UV','Progress','Shape','Tint']:
    p=unreal.CustomInput();p.set_editor_property('input_name',name);inputs.append(p)
n.set_editor_property('inputs',inputs)
e.connect_material_expressions(uv,'',n,'UV');e.connect_material_expressions(color,'RGB',n,'Tint')
for name in ['Progress','Shape']:e.connect_material_expressions(scalar(name,0),'',n,name)
n.set_editor_property('code',r'''
float2 p=(UV-.5)*2.;
float2 q=float2(dot(p,float2(.82,.572)),dot(p,float2(-.572,.82)));
float arc=(Shape>1.5&&Shape<2.5)? .48*q.x*q.x: .10*q.x*q.x;
float taper=pow(saturate(1.-abs(q.x)/.93),.65);
float width=(.035+.028*Progress)*taper;
float grain=sin(q.x*71.+q.y*18.)*.003*taper;
float d=abs(q.y-arc+grain);
float ink=(1.-smoothstep(width+.025,width+.05,d))*step(abs(q.x),.94);
float body=(1.-smoothstep(width,width+.012,d))*step(abs(q.x),.93);
float core=(1.-smoothstep(.005*taper+.001,.017*taper+.006,d))*step(abs(q.x),.91);
float echo=(1.-smoothstep(.006,.018,abs(q.y-arc+.105*taper)))*taper*.55;
if(Shape>2.5&&Shape<3.5){
 float t=pow(saturate(1.-abs(q.y)/.74),.7);
 float cross=(1.-smoothstep(.025*t,.045*t+.003,abs(q.x)))*step(abs(q.y),.74);
 ink=max(ink,cross);body=max(body,cross);core=max(core,cross*.75);
}
float shards=0.;
for(int i=0;i<4;i++){
 float x=-.60+i*.38;
 float2 v=q-float2(x,.16+sin(i*2.1)*.12);
 float len=.045+.018*i;
 shards=max(shards,(1.-smoothstep(.007,.018,abs(v.y-v.x*.28)))*saturate(1.-abs(v.x)/len));
}
float charge=smoothstep(.52,1.,Progress);
float star=exp(-abs(q.x)*60.-abs(q.y)*9.)*charge;
float alpha=saturate(max(ink,max(echo,shards*.65))+star);
float3 rgb=lerp(float3(.012,.015,.024),Tint*(.85+charge*.65),body);
rgb=lerp(rgb,float3(1.,.97,.84)*(1.35+charge),core);
rgb+=Tint*(echo*.8+shards*.8)+star*float3(1.,.93,.7)*1.6;
return float4(rgb,alpha);
''')
rgb=e.create_material_expression(m,unreal.MaterialExpressionComponentMask);rgb.set_editor_property('r',True);rgb.set_editor_property('g',True);rgb.set_editor_property('b',True)
alpha=e.create_material_expression(m,unreal.MaterialExpressionComponentMask);alpha.set_editor_property('r',False);alpha.set_editor_property('g',False);alpha.set_editor_property('b',False);alpha.set_editor_property('a',True)
assert e.connect_material_expressions(n,'',rgb,'')
assert e.connect_material_expressions(n,'',alpha,'')
tex=e.create_material_expression(m,unreal.MaterialExpressionTextureSampleParameter2D)
tex.set_editor_property('parameter_name','Icon');tex.set_editor_property('texture',unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
mix=e.create_material_expression(m,unreal.MaterialExpressionLinearInterpolate)
e.connect_material_expressions(alpha,'',mix,'A');e.connect_material_expressions(tex,'A',mix,'B');e.connect_material_expressions(scalar('UseIcon',0),'',mix,'Alpha')
fade=e.create_material_expression(m,unreal.MaterialExpressionMultiply)
e.connect_material_expressions(mix,'',fade,'A');e.connect_material_expressions(scalar('RenderOpacity',1),'',fade,'B')
icon_color=e.create_material_expression(m,unreal.MaterialExpressionLinearInterpolate)
e.connect_material_expressions(rgb,'',icon_color,'A');e.connect_material_expressions(color,'RGB',icon_color,'B')
e.connect_material_expressions(scalar('UseIcon',0),'',icon_color,'Alpha')
e.connect_material_property(icon_color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
e.connect_material_property(fade,'',unreal.MaterialProperty.MP_OPACITY)
e.recompile_material(m)
assert unreal.EditorAssetLibrary.save_loaded_asset(m), 'Material save failed'
unreal.log_warning('BLADE_READABILITY_REFINED_SAVED')
