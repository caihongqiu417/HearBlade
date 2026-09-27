import bpy
OUT='D:/Udev/HearBlade/SourceArt/Environment/HarmonicCourtV2'
m=bpy.data.materials['HC_Ivory'];n=m.node_tree.nodes;l=m.node_tree.links
tex=n.new('ShaderNodeTexImage');tex.name='LimestoneDetail';tex.image=bpy.data.images.load(OUT+'/T_HC_Limestone_Detail.png',check_existing=True);tex.projection='BOX';tex.projection_blend=.25
pos=n.new('ShaderNodeNewGeometry');scale=n.new('ShaderNodeVectorMath');scale.operation='SCALE';scale.inputs['Scale'].default_value=.5
l.new(pos.outputs['Position'],scale.inputs[0]);l.new(scale.outputs['Vector'],tex.inputs['Vector']);l.new(tex.outputs['Color'],n.get('Principled BSDF').inputs['Base Color'])
bpy.ops.wm.save_as_mainfile(filepath=OUT+'/HarmonicCourtV2.blend')
bpy.context.scene.render.filepath=OUT+'/HarmonicCourtV2_Model.png';bpy.ops.render.render(write_still=True)
print('LIMESTONE_APPLIED')
