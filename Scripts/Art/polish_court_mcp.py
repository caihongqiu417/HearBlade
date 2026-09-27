import bpy,math
from mathutils import Vector
from math import pi,sin,cos
OUT='D:/Udev/HearBlade/SourceArt/Environment/HarmonicCourtV2'
stage=bpy.data.objects['SM_HC2_Static_000'];parts=[stage]
def native_cone(name,loc,r1,r2,h,mat,n=24):
    bpy.ops.mesh.primitive_cone_add(vertices=n,radius1=r1,radius2=r2,depth=h,location=loc)
    o=bpy.context.object;o.name=name;o.data.materials.append(bpy.data.materials[mat]);parts.append(o)
    for poly in o.data.polygons:poly.use_smooth=True
    return o
# Extend support surface to the parapet: only a 2 cm transition at the paving edge.
native_cone('OuterWalkwaySupport',(0,0,-.12),19.35,19.35,.2,'HC_Ivory',128)
# Cluster rock spires around each floating island to break the single cone silhouette.
for i in range(15):
    a=.05+i*pi/14;r=48+(i%3)*6;x,y=r*cos(a),r*sin(a);h=8+(i%5)*2.2
    for j in range(5):
        b=j*2*pi/5;rr=2.7+.2*(j%2);height=h*(.7+.09*j)
        o=native_cone('LimestonePillar',(x+cos(b)*rr,y+sin(b)*rr,h*.74-height/2),.24,1.25,height,'HC_Stone',24)
        o.rotation_euler.x=.06*sin(i+j);o.rotation_euler.y=.07*cos(i+j)
        native_cone('PillarPaleCap',(x+cos(b)*rr,y+sin(b)*rr,h*.75),1.25,1.1,.5,'HC_Stone',32)
# Inset gold diamond crests and reinforcing feet on each tower.
for a in [.2,.9,1.57,2.24,2.94,3.75,5.65]:
    x,y=18.8*cos(a),18.8*sin(a);normal=Vector((-cos(a),-sin(a),0))
    p=Vector((x,y,6.08))+normal*1.09
    bpy.ops.mesh.primitive_torus_add(major_radius=.22,minor_radius=.025,major_segments=4,minor_segments=6,location=p)
    o=bpy.context.object;o.name='GoldDiamondHeraldry';o.rotation_euler=Vector((0,0,1)).rotation_difference(normal).to_euler();o.data.materials.append(bpy.data.materials['HC_Gold']);parts.append(o)
    for offset in [-1,1]:
        tangent=Vector((-sin(a),cos(a),0));loc=Vector((x,y,.82))+tangent*offset*.97+normal*.9
        o=native_cone('TowerArmoredCorner',loc,.29,.13,.7,'HC_Gold',4)
bpy.ops.object.select_all(action='DESELECT')
for o in parts:o.select_set(True)
bpy.context.view_layer.objects.active=stage;bpy.ops.object.join()
bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
bpy.ops.export_scene.fbx(filepath=OUT+'/SM_HC2_Static_000.fbx',use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',add_leaf_bones=False)
# A brighter preview world without changing the game lighting.
w=bpy.context.scene.world;w.use_nodes=True;w.node_tree.nodes.get('Background').inputs['Color'].default_value=(.34,.48,.65,1);w.node_tree.nodes.get('Background').inputs['Strength'].default_value=.65
bpy.ops.wm.save_as_mainfile(filepath=OUT+'/HarmonicCourtV2.blend')
bpy.context.scene.render.filepath=OUT+'/HarmonicCourtV2_Model.png';bpy.ops.render.render(write_still=True)
print('HC2_POLISHED')
