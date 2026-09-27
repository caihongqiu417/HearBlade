import bpy,math
from math import sin,cos,pi
OUT='D:/Udev/HearBlade/SourceArt/Environment/HarmonicCourtV2'
stage=bpy.data.objects['SM_HC2_Static_000'];colliders=[]
def keep(o):
    o.name='UCX_SM_HC2_Static_000_'+str(len(colliders)).zfill(2);o.hide_render=True;o.display_type='WIRE';colliders.append(o)
bpy.ops.mesh.primitive_cylinder_add(vertices=64,radius=19.35,depth=.3,location=(0,0,-.15));keep(bpy.context.object)
for i in range(32):
    a=i*2*pi/32;bpy.ops.mesh.primitive_cube_add(size=1,location=(19.2*cos(a),19.2*sin(a),.7));o=bpy.context.object;o.dimensions=(3.7,.75,1.4);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.rotation_euler.z=a+pi/2;keep(o)
for a in [.2,.9,1.57,2.24,2.94,3.75,5.65]:
    bpy.ops.mesh.primitive_cylinder_add(vertices=8,radius=1.4,depth=6.8,location=(18.8*cos(a),18.8*sin(a),3.4));keep(bpy.context.object)
bpy.ops.object.select_all(action='DESELECT');stage.select_set(True)
for o in colliders:o.select_set(True)
bpy.context.view_layer.objects.active=stage
bpy.ops.export_scene.fbx(filepath=OUT+'/SM_HC2_Static_000.fbx',use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',add_leaf_bones=False)
bpy.ops.wm.save_as_mainfile(filepath=OUT+'/HarmonicCourtV2.blend')
print('UCX_AUTHORED',len(colliders))
