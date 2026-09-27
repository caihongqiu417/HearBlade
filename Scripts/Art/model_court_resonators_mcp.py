import bpy,math,json,os
from math import pi,sin,cos
from mathutils import Vector
OUT='D:/Udev/HearBlade/SourceArt/Environment/CourtResonators';os.makedirs(OUT,exist_ok=True)
gold=bpy.data.materials['HC_Gold'];blue=bpy.data.materials['HC_Blue'];ivory=bpy.data.materials['HC_Ivory']
parts=[]
def add(o,name,m):
    o.name=name;o.data.materials.append(m)
    for p in o.data.polygons:p.use_smooth=True
    parts.append(o);return o
def cyl(name,loc,r,dep,m):
    bpy.ops.mesh.primitive_cylinder_add(vertices=64,radius=r,depth=dep,location=loc);o=bpy.context.object
    mod=o.modifiers.new('FineBevel','BEVEL');mod.width=.018;mod.segments=3;bpy.ops.object.modifier_apply(modifier=mod.name)
    return add(o,name,m)
def tor(name,loc,r,t,m,vertical=False):
    bpy.ops.mesh.primitive_torus_add(major_radius=r,minor_radius=t,major_segments=96,minor_segments=12,location=loc);o=bpy.context.object
    if vertical:o.rotation_euler.x=pi/2
    return add(o,name,m)
def join_export(name,pivot):
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts:o.select_set(True)
    bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();o=bpy.context.object;o.name=name;bpy.context.scene.cursor.location=pivot;bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    o.location=(0,0,0)
    bpy.ops.export_scene.fbx(filepath=OUT+'/'+name+'.fbx',use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',add_leaf_bones=False)
    parts.clear();return o
# Low stepped plinth and a fork supporting a suspended metronome.
cyl('StonePlinth',(0,0,.12),.62,.24,ivory);cyl('GoldPlinthLip',(0,0,.26),.65,.05,gold)
cyl('DarkRecess',(0,0,.32),.48,.08,blue)
for x in [-.48,.48]:
    cyl('TunedColumn',(x,0,.9),.055,1.15,gold)
tor('FixedOuterHoop',(0,0,1.25),.51,.027,gold,True)
base=join_export('SM_Court_MetronomeBase',(0,0,0))
# Moving concentric hoops, separate pivot through their common centre.
tor('InnerTuningRing',(0,0,0),.37,.021,gold,True)
tor('InnerInlay',(0,-.012,0),.335,.009,blue,True)
for x in [-.39,.39]:cyl('GimbalStud',(x,0,0),.04,.08,gold)
ring=join_export('SM_Court_TuningRing',(0,0,0))
# Pendulum pivot at its top; actual ball and shaft swing together.
cyl('PendulumShaft',(0,0,-.24),.013,.48,gold)
bpy.ops.mesh.primitive_uv_sphere_add(segments=32,ring_count=16,radius=.115,location=(0,0,-.49));add(bpy.context.object,'PendulumWeight',gold)
pendulum=join_export('SM_Court_Pendulum',(0,0,0))
# Peripheral concentric floor resonators; central combat space is untouched.
for r,t in [(.72,.028),(.61,.016),(.45,.01)]:tor('ResonanceRing',(0,0,.03),r,t,gold)
cyl('ResonanceMedallion',(0,0,.015),.25,.03,blue)
disc=join_export('SM_Court_ResonanceDisc',(0,0,0))
for o in [base,ring,pendulum,disc]:o.hide_render=True
# Save as a separate source file, preserving the main arena source.
bpy.ops.wm.save_as_mainfile(filepath=OUT+'/CourtResonators.blend')
print('COURT_RESONATORS_MODELED')
