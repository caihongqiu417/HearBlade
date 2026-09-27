import bpy
OUT='D:/Udev/HearBlade/SourceArt/Environment/HarmonicCourtV2'
stage=bpy.data.objects['SM_HC2_Static_000']
for p in stage.data.polygons:
    if p.center.z < .12 and p.normal.z > .7:p.use_smooth=False
for name in ['HC_Foliage','HC_Stone','HC_Flower']:
    p=bpy.data.materials[name].node_tree.nodes.get('Principled BSDF');p.inputs['Roughness'].default_value=.9;p.inputs['Specular IOR Level'].default_value=.15
bpy.ops.object.select_all(action='DESELECT');stage.select_set(True)
for o in bpy.context.scene.objects:
    if o.name.startswith('UCX_SM_HC2_Static_000_'):o.select_set(True)
bpy.context.view_layer.objects.active=stage
bpy.ops.export_scene.fbx(filepath=OUT+'/SM_HC2_Static_000.fbx',use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',add_leaf_bones=False)
bpy.ops.wm.save_as_mainfile(filepath=OUT+'/HarmonicCourtV2.blend')
bpy.context.scene.render.filepath=OUT+'/HarmonicCourtV2_Model.png';bpy.ops.render.render(write_still=True)
print('SURFACE_NORMALS_FINISHED')
