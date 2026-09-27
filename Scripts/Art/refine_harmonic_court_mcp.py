"""Run through Blender MCP execute_blender_code; native Blender modeling/baking."""
import bpy, bmesh, math, json, os
from mathutils import Vector
from math import pi, sin, cos
OUT='D:/Udev/HearBlade/SourceArt/Environment/HarmonicCourtV2'
os.makedirs(OUT,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
for m in list(bpy.data.materials):
    if m.name.startswith('HC_'):bpy.data.materials.remove(m)
def mat(name,color,metal=0,glow=0):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*color,1)
    p.inputs['Metallic'].default_value=metal;p.inputs['Roughness'].default_value=.4
    p.inputs['Emission Color'].default_value=(*color,1);p.inputs['Emission Strength'].default_value=glow
    return m
ivory=mat('HC_Ivory',(.43,.44,.415));gold=mat('HC_Gold',(.48,.33,.15),.8)
blue=mat('HC_Blue',(.025,.047,.078),.35);cyan=mat('HC_Cyan',(.045,.25,.33),.25,.7)
dark=mat('HC_Speaker',(.008,.019,.026));stone=mat('HC_Stone',(.26,.285,.31));green=mat('HC_Foliage',(.045,.085,.055));purple=mat('HC_Flower',(.16,.055,.12))
def finish(o,name,m,bevel=.04):
    o.name=name;o.data.materials.append(m)
    if bevel:
        mod=o.modifiers.new('Edge craftsmanship','BEVEL');mod.width=bevel;mod.segments=3
        bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=mod.name)
    for face in o.data.polygons:face.use_smooth=True
    mod=o.modifiers.new('Weighted architectural normals','WEIGHTED_NORMAL');mod.keep_sharp=True;mod.weight=50
    bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=mod.name)
    return o
def box(name,loc,size,m,bevel=.04,angle=0):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.rotation_euler.z=angle
    return finish(o,name,m,bevel)
def cyl(name,loc,r,d,m,n=64):
    bpy.ops.mesh.primitive_cylinder_add(vertices=n,radius=r,depth=d,location=loc)
    return finish(bpy.context.object,name,m,.025)
def cone(name,loc,r1,r2,d,m,n=16):
    bpy.ops.mesh.primitive_cone_add(vertices=n,radius1=r1,radius2=r2,depth=d,location=loc)
    return finish(bpy.context.object,name,m,.025)
def tor(name,loc,r,t,m,normal=None):
    bpy.ops.mesh.primitive_torus_add(major_radius=r,minor_radius=t,major_segments=96,minor_segments=8,location=loc);o=bpy.context.object
    if normal is not None:o.rotation_euler=Vector((0,0,1)).rotation_difference(normal).to_euler()
    return finish(o,name,m,0)
def arc(name,loc,r,t,start,end,m,vertical=False,flat=None):
    # Cut and cap a native Blender torus; no hand-authored vertex buffers.
    bpy.ops.mesh.primitive_torus_add(major_radius=r,minor_radius=t,major_segments=192,minor_segments=4,location=loc)
    o=bpy.context.object;bm=bmesh.new();bm.from_mesh(o.data)
    doomed=[v for v in bm.verts if not start<=math.atan2(v.co.y,v.co.x)<=end]
    bmesh.ops.delete(bm,geom=doomed,context='VERTS');bmesh.ops.holes_fill(bm,edges=[e for e in bm.edges if e.is_boundary],sides=0)
    bm.to_mesh(o.data);bm.free()
    if flat is not None:o.scale.z=flat/t
    if vertical:o.rotation_euler.x=pi/2
    return finish(o,name,m,.015 if flat is None else 0)
def join(parts,name,pivot):
    bpy.ops.object.select_all(action='DESELECT')
    for p in parts:p.select_set(True)
    bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();o=bpy.context.object;o.name=name
    bpy.context.scene.cursor.location=pivot;bpy.ops.object.origin_set(type='ORIGIN_CURSOR');return o
moving=[]
def motion(o,mode):moving.append((o,mode));return o
# Circular tiled arena with blue curved inlay, concentric edging and a sun seal.
cyl('Foundation',(0,0,-.5),18,.8,stone,128);cyl('IvoryPaving',(0,0,-.1),17.6,.2,ivory,128)
for r in [3.0,3.15,5.7,6.0,12.8,13.1,16.65,17.4]:tor('FloorGoldInlay',(0,0,.012),r,.027,gold)
for i in range(12):
    a=-pi+(i+.5)*2*pi/12
    arc('BlueMarbleSector',(0,0,.008),9.4,3.35,a-.205,a+.205,blue,flat=.008)
    for b in [a-.205,a+.205]:box('SectorRadialGold',(9.4*cos(b),9.4*sin(b),.018),(6.7,.055,.02),gold,.005,b)
for i in range(48):
    a=i*2*pi/48
    box('FinePaverSeam',(14.9*cos(a),14.9*sin(a),.006),(3.0,.018,.008),stone,.002,a)
    box('PerimeterBlueTile',(16.9*cos(a),16.9*sin(a),.012),(.58,1.65,.018),blue,.006,a)
for i in range(-12,13):
    d=i*1.25;span=2*math.sqrt(max(0,16.55**2-d*d))
    box('PavingGroutX',(d,0,.029),(.008,span,.003),stone,.001)
    box('PavingGroutY',(0,d,.029),(span,.008,.003),stone,.001)
cyl('BlueSunCenter',(0,0,.011),2.95,.015,blue,128)
for i in range(8):
    a=i*pi/4
    # Flat four-sided spires create sharp compass rays, not plain rectangular bars.
    o=cone('CompassRay',(cos(a)*1.6,sin(a)*1.6,.032),.45,0,3.4,gold,4);o.rotation_euler=(0,pi/2,a);o.scale.z=1
    o.scale.x=.025
# Multi-layer speaker monuments oriented toward the arena.
for i,a in enumerate([.2,.9,1.57,2.24,2.94,3.75,5.65]):
    x,y=18.8*cos(a),18.8*sin(a);normal=Vector((-cos(a),-sin(a),0));tangent=Vector((-sin(a),cos(a),0));ang=a-pi/2
    base=Vector((x,y,0))
    def face(name,side,depth,z,size,m,bev=.035):return box(name,base+tangent*side+normal*depth+Vector((0,0,z)),size,m,bev,ang)
    cyl('OctagonalFoot',(x,y,.22),1.65,.44,stone,32);cyl('GoldFoot',(x,y,.51),1.48,.15,gold,32)
    box('TowerBody',(x,y,3.55),(2.2,1.9,6.05),ivory,.14,ang)
    face('RecessedEnamelPanel',0,.99,3.65,(1.78,.12,5.5),blue,.10)
    for s in [-1,1]:
        face('GoldVerticalMoulding',s*.99,1.02,3.65,(.09,.14,5.65),gold)
        face('SideFluting',s*.83,1.075,3.6,(.025,.025,5.2),ivory,.006)
    for z in [2.15,4.7]:
        c=base+normal*1.10+Vector((0,0,z))
        tor('SpeakerGoldMount',c,.79,.065,gold,normal)
        tor('SpeakerCyanRim',c+normal*.035,.70,.023,cyan,normal)
        parts=[]
        o=cone('RecessedSpeakerCone',c+normal*.018,.66,.30,.14,dark,64);o.rotation_euler=Vector((0,0,1)).rotation_difference(normal).to_euler();parts.append(o)
        o=cyl('SpeakerDustCap',c+normal*.115,.205,.065,cyan);o.rotation_euler=Vector((0,0,1)).rotation_difference(normal).to_euler();parts.append(o)
        for r in [.42,.53,.61]:parts.append(tor('ConeConcentricRidge',c+normal*.07,r,.012,stone,normal))
        motion(join(parts,'SpeakerAssembly',c),'BeatSpeaker')
        for j in range(8):
            p=c+tangent*(cos(j*pi/4)*.86)+Vector((0,0,sin(j*pi/4)*.86))
            o=cyl('RimFastener',p,.035,.04,gold,8);o.rotation_euler=Vector((0,0,1)).rotation_difference(normal).to_euler()
    # Center crest and physical vertical light rails (no equalizer texture).
    for s in [-.12,0,.12]:face('ResonanceVent',s,1.09,3.35,(.045,.035,.68),cyan,.01)
    crownparts=[box('CrownCornice',(x,y,6.75),(2.5,2.2,.26),gold,.065,ang),box('CrownInset',(x,y,6.95),(2.25,1.95,.13),blue,.03,ang)]
    for j in range(6):
        b=j*2*pi/6;crownparts.append(cone('CrownProng',(x+cos(b)*.91,y+sin(b)*.91,7.30),.15,0,1.15,gold,16))
        crownparts.append(tor('ProngCollar',(x+cos(b)*.91,y+sin(b)*.91,6.98),.16,.026,gold))
    motion(join(crownparts,'CrownedCap',(x,y,6.75)),'BeatCrown')
    crystal=join([cone('CrystalUpper',(x,y,8.2),.42,0,1.1,gold),cone('CrystalLower',(x,y,7.45),0,.42,.4,gold)],'FloatingDiamond',(x,y,7.9));motion(crystal,'BeatShard')
# Segmented curved halo: gold structural edge, ivory stone face, inner luminous rail.
hc=Vector((0,21,10))
for i in range(12):
    a=-pi+(i+.5)*2*pi/12;s=a-.235;e=a+.235
    parts=[arc('HaloGoldRib',hc,6.2,.66,s,e,gold,True),arc('HaloIvoryFacing',hc+Vector((0,-.13,0)),6.2,.54,s+.015,e-.015,ivory,True)]
    parts.append(arc('HaloInnerEnamel',hc+Vector((0,-.1,0)),5.61,.075,s,e,blue,True))
    parts.append(arc('HaloCyanChannel',hc+Vector((0,-.18,0)),5.62,.031,s,e,cyan,True))
    motion(join(parts,'CarvedHaloSegment',hc),'BeatHalo')
    p=hc+Vector((cos(a)*7.35,0,sin(a)*7.35));motion(cone('OrbitDiamond',p,.23,0,.65,gold),'BeatShard')
# Continuous parapet, piers and planters frame the arena without blocking combat.
for i in range(32):
    a=i*2*pi/32;x,y=19.2*cos(a),19.2*sin(a);ang=a+pi/2
    box('ParapetStone',(x,y,.65),(3.55,.55,1.3),ivory,.08,ang)
    box('ParapetGoldCap',(x,y,1.34),(3.6,.68,.14),gold,.03,ang)
    p=Vector((x,y,.72))-Vector((cos(a),sin(a),0))*.31
    box('BlueBalustradePanel',p,(3.22,.06,.85),blue,.03,ang)
    box('CyanBalustradeLine',p+Vector((0,0,.32)),(3.05,.08,.025),cyan,.005,ang)
    if i%2==0:
        cone('PierCap',(x,y,1.7),.27,0,.65,gold)
        box('StonePier',(x,y,.85),(.48,.8,1.7),ivory,.05,ang)
# Outer architectural arcades and a rear cathedral silhouette.
for a in [.12,.50,.88,1.26,1.64,2.02,2.40,2.78,3.16]:
    x,y=31*cos(a),31*sin(a);ang=a-pi/2
    box('ArcadeColumn',(x,y,5),(1.25,1.25,10),ivory,.12)
    box('ArcadeCapital',(x,y,10),(1.7,1.7,.4),gold,.06)
    cone('ArcadePinnacle',(x,y,12),.5,.06,3.5,ivory,24)
    # Real curved arch between columns, opening maintained.
    mid=a+.19;loc=(31*cos(mid),31*sin(mid),7.5)
    arch=arc('OuterArcadeArch',loc,5.2,.45,0,pi,ivory,True);arch.rotation_euler.z=mid-pi/2
    trim=arc('OuterArcadeArchTrim',loc,5.65,.065,0,pi,gold,True);trim.rotation_euler.z=mid-pi/2
    box('UpperGallery',(31*cos(mid),31*sin(mid),12.7),(11.6,1.6,.5),ivory,.06,mid-pi/2)
for x,z,h in [(-3.5,10,20),(0,15,30),(3.5,10,20)]:
    box('CathedralSpine',(x,39,z),(2.2,2.6,h),ivory,.12)
    cone('CathedralSpire',(x,39,z+h/2+3.5),1.15,.06,7,gold,32)
    box('CathedralBlueInset',(x,37.62,z),(1.2,.1,h-2),blue,.04)
    box('CathedralGoldRib',(x,37.52,z),(.08,.09,h-1),gold,.015)
    for side in [-1,1]:
        box('CathedralButtress',(x+side*1.3,39,z*.7),(.55,3.3,h*.7),ivory,.06)
        cone('ButtressSpire',(x+side*1.3,38.5,h*.72+1),.4,0,2,gold,4)
    for zh in [h*.3,h*.55,h*.8]:
        box('CathedralCornice',(x,39,zh),(2.65,2.95,.25),gold,.04)
# Faceted distant floating limestone cliffs with planted tops.
for i in range(15):
    a=.05+i*pi/14;r=48+(i%3)*6;x,y=r*cos(a),r*sin(a);h=8+(i%5)*2.2
    rock=cone('FloatingCliff',(x,y,h*.35),1.6,4.8,h,stone,24)
    sub=rock.modifiers.new('GeologicalCuts','SUBSURF');sub.subdivision_type='SIMPLE';sub.levels=3
    bpy.context.view_layer.objects.active=rock;bpy.ops.object.modifier_apply(modifier=sub.name)
    tex=bpy.data.textures.new('CliffStrata',type='CLOUDS');tex.noise_scale=1.8
    dis=rock.modifiers.new('RockErosion','DISPLACE');dis.texture=tex;dis.strength=1.7
    bpy.ops.object.modifier_apply(modifier=dis.name)
    cyl('LimestoneTop',(x,y,h*.85),4.8,.65,stone,48)
    for j in range(4):
        b=j*pi/2;px=x+cos(b)*2.5;py=y+sin(b)*2.5
        cyl('TreeTrunk',(px,py,h*.85+1.1),.16,1.8,stone,16)
        for ox,oy,oz,rr in [(0,0,0,1.25),(.65,.2,-.3,.9),(-.45,-.35,-.25,.85)]:
            bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3,radius=rr,location=(px+ox,py+oy,h*.85+2.2+oz));leaf=finish(bpy.context.object,'TreeCanopy',green,0)
            tex=bpy.data.textures.new('LeafClusters',type='CLOUDS');tex.noise_scale=.22
            dis=leaf.modifiers.new('FoliageContour','DISPLACE');dis.texture=tex;dis.strength=.28
            bpy.context.view_layer.objects.active=leaf;bpy.ops.object.modifier_apply(modifier=dis.name)
for i in range(12):
    a=i*2*pi/12;x,y=22*cos(a),22*sin(a)
    box('GardenPlanter',(x,y,.45),(2.5,1.6,.9),ivory,.12,a)
    for j in range(5):
        p=(x+cos(j*2.4)*.85,y+sin(j*2.4)*.5,1.05)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3,radius=.55,location=p);finish(bpy.context.object,'Shrub',green,0)
        bpy.ops.mesh.primitive_uv_sphere_add(segments=12,ring_count=8,radius=.12,location=(p[0],p[1],1.5));finish(bpy.context.object,'Flowers',purple,0)
# Fine architectural mouldings and restrained gilt scrollwork.
def filigree(name,points,radius=.018,material=gold):
    curve=bpy.data.curves.new(name,'CURVE');curve.dimensions='3D';curve.resolution_u=12;curve.bevel_depth=radius;curve.bevel_resolution=3
    spline=curve.splines.new('BEZIER');spline.bezier_points.add(len(points)-1)
    for bp,co in zip(spline.bezier_points,points):bp.co=co;bp.handle_left_type='AUTO';bp.handle_right_type='AUTO'
    obj=bpy.data.objects.new(name,curve);bpy.context.collection.objects.link(obj);curve.materials.append(material)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj;bpy.ops.object.convert(target='MESH')
    return bpy.context.object
for a in [.2,.9,1.57,2.24,2.94,3.75,5.65]:
    base=Vector((18.8*cos(a),18.8*sin(a),0));normal=Vector((-cos(a),-sin(a),0));tan=Vector((-sin(a),cos(a),0))
    for side in [-1,1]:
        def p(u,z):return base+tan*side*1.115+normal*u+Vector((0,0,z))
        filigree('SidePanelBeadedBorder',[p(-.65,1),p(-.65,5.95),p(0,6.2),p(.65,5.95),p(.65,1),p(0,.8),p(-.65,1)],.025)
        for sign in [-1,1]:
            filigree('GiltAcanthusScroll',[p(0,2),p(sign*.38,2.65),p(sign*.45,3.35),p(sign*.18,3.6),p(sign*.04,3.4),p(sign*.22,3.28)],.017)
            filigree('UpperAcanthusScroll',[p(0,5.4),p(sign*.35,4.95),p(sign*.43,4.35),p(sign*.16,4.05),p(0,4.23)],.017)
        filigree('CentralStem',[p(0,1.25),p(.08,2.1),p(0,3.7),p(-.07,5),p(0,5.8)],.017)
    for depth in [-.65,-.35,0,.35,.65]:
        pos=base+tan*1.14+normal*depth+Vector((0,0,3.5));cyl('ColumnFineFluting',pos,.028,4.9,gold,12)
for i in range(96):
    a=i*2*pi/96;cyl('RosetteBeading',(3.08*cos(a),3.08*sin(a),.035),.022,.018,gold,12)
for i in range(32):
    a=i*2*pi/32;normal=Vector((-cos(a),-sin(a),0));tan=Vector((-sin(a),cos(a),0));c=Vector((19.2*cos(a),19.2*sin(a),.73))+normal*.355
    for offset in [-.9,0,.9]:
        mid=c+tan*offset
        filigree('BalustradeLozenge',[mid-tan*.25,mid+Vector((0,0,.25)),mid+tan*.25,mid-Vector((0,0,.25)),mid-tan*.25],.012)
for x,h in [(-3.5,20),(0,30),(3.5,20)]:
    for z in [3,h*.42,h*.7]:
        filigree('CathedralLancet',[(x-.55,37.48,z),(x-.55,37.48,z+2),(x,37.48,z+3.3),(x+.55,37.48,z+2),(x+.55,37.48,z)],.045)
# Shared static structure; separate animated assemblies retain meaningful pivots.
static=[o for o in bpy.context.scene.objects if o.type=='MESH' and o not in [x[0] for x in moving]]
stage=join(static,'Architecture',(0,0,0));objects=[(stage,'Static')]+moving;manifest=[]
for i,(o,mode) in enumerate(objects):
    o.name='SM_HC2_'+mode+'_'+str(i).zfill(3)
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    loc=o.location.copy();o.location=(0,0,0)
    bpy.ops.export_scene.fbx(filepath=OUT+'/'+o.name+'.fbx',use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',add_leaf_bones=False)
    o.location=loc;manifest.append({'name':o.name,'mode':mode,'location':list(loc)})
with open(OUT+'/manifest.json','w') as f:json.dump(manifest,f,indent=2)
# Procedural Blender stone/enamel/metal surface networks, later baked for UE.
for m,base,rough in [(ivory,(.43,.44,.415),.52),(blue,(.025,.047,.078),.3),(gold,(.48,.33,.15),.3)]:
    n=m.node_tree.nodes;l=m.node_tree.links;p=n.get('Principled BSDF')
    geom=n.new('ShaderNodeNewGeometry');noise=n.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=3.5;noise.inputs['Detail'].default_value=4;noise.inputs['Roughness'].default_value=.72
    l.new(geom.outputs['Position'],noise.inputs['Vector'])
    ramp=n.new('ShaderNodeValToRGB');ramp.color_ramp.elements[0].position=.22;ramp.color_ramp.elements[0].color=(*(c*.68 for c in base),1)
    ramp.color_ramp.elements[1].position=.8;ramp.color_ramp.elements[1].color=(*(min(1,c*1.24) for c in base),1)
    l.new(noise.outputs['Fac'],ramp.inputs[0]);l.new(ramp.outputs['Color'],p.inputs['Base Color'])
    bump=n.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.14;bump.inputs['Distance'].default_value=.025;l.new(noise.outputs['Fac'],bump.inputs['Height']);l.new(bump.outputs['Normal'],p.inputs['Normal']);p.inputs['Roughness'].default_value=rough
bpy.ops.object.light_add(type='SUN',location=(0,0,30));bpy.context.object.rotation_euler=(.4,-.5,-.4);bpy.context.object.data.energy=2.1
bpy.ops.object.light_add(type='AREA',location=(0,-10,22));bpy.context.object.data.energy=3300;bpy.context.object.data.size=24
bpy.ops.object.camera_add(location=(23,-31,16));cam=bpy.context.object;cam.rotation_euler=(Vector((0,8,5))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.lens=31;bpy.context.scene.camera=cam
s=bpy.context.scene;s.world.color=(.35,.45,.65);s.render.engine='CYCLES';s.cycles.samples=32;s.render.resolution_x=1600;s.render.resolution_y=1000;s.render.resolution_percentage=100
s.render.filepath=OUT+'/HarmonicCourtV2_Model.png';bpy.ops.wm.save_as_mainfile(filepath=OUT+'/HarmonicCourtV2.blend');bpy.ops.render.render(write_still=True)
print('HC2_MODELED',len(objects),sum(len(o.data.polygons) for o,_ in objects))
