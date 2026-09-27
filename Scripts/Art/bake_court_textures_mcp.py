import bpy,os
OUT='D:/Udev/HearBlade/SourceArt/Environment/HarmonicCourtV2'
bpy.ops.object.select_all(action='DESELECT')
bpy.ops.mesh.primitive_plane_add(size=2,location=(0,0,-100));plane=bpy.context.object;plane.name='TextureBakePlane'
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=8
for name,roughness in [('HC_Ivory',.58),('HC_Blue',.32),('HC_Gold',.36)]:
    material=bpy.data.materials[name];plane.data.materials.clear();plane.data.materials.append(material)
    nodes=material.node_tree.nodes;links=material.node_tree.links;out=next(n for n in nodes if n.type=='OUTPUT_MATERIAL');p=nodes.get('Principled BSDF')
    base_link=p.inputs['Base Color'].links[0].from_socket
    for channel in ['Color','Roughness']:
        image=bpy.data.images.new('T_'+name+'_'+channel,width=1024,height=1024,alpha=False)
        image.colorspace_settings.name='sRGB' if channel=='Color' else 'Non-Color'
        node=nodes.new('ShaderNodeTexImage');node.image=image;nodes.active=node
        emit=nodes.new('ShaderNodeEmission')
        if channel=='Color':links.new(base_link,emit.inputs['Color'])
        else:
            emit.inputs['Color'].default_value=(roughness,roughness,roughness,1)
        links.new(emit.outputs[0],out.inputs['Surface'])
        bpy.ops.object.bake(type='EMIT',margin=16)
        image.filepath_raw=OUT+'/'+image.name+'.png';image.file_format='PNG';image.save()
        nodes.remove(emit);nodes.remove(node);links.new(p.outputs[0],out.inputs['Surface'])
bpy.data.objects.remove(plane,do_unlink=True)
bpy.ops.wm.save_as_mainfile(filepath=OUT+'/HarmonicCourtV2.blend')
print('HC2_TEXTURES_BAKED')
