"""Area-attack warning: bigger and oppressive instead of bright.

1. Rebuilds M_GroundBurstWarning with a dark "pressure pool" that deepens as the
   hit approaches, a thin ember rim, a wall closing in from the edge and rim teeth.
   Colours/brightness are driven at runtime by AttackReadabilityComponent.
2. Sets every GroundBurst radius on the Dragon Knight attack data to TARGET_RADIUS.
   The area is escaped by dodging on the detonation beat (dodge-through), so the
   radius is no longer shrunk to a walkable exit.

Run:  py D:/Udev/HearBlade/Scripts/oppressive_ground_warning.py
"""
import unreal

ROOT = '/Game/Combat/Readability'
ATTACK_DIR = '/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS'
TARGET_RADIUS = 1400.  # cm; too wide to outrun in the warning time -> dodge through on the beat
e = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

CODE = r'''
float2 p=(UV-.5)*2;float r=length(p);float a=atan2(p.y,p.x);
float inside=step(r,1.);
float pool=(.30+.40*Progress)*(.55+.45*smoothstep(.15,1.,r));
float edge=1-smoothstep(.006,.02,abs(r-.985));
float wall=(1-smoothstep(0.,.045,abs(r-(1.-.88*Progress))))*.55;
float teeth=step(.5,frac((a+3.14159)/6.28318*28))*step(.90,r)*(1-smoothstep(.95,.975,r))*.7;
float shock=(1-smoothstep(.015,.07,abs(r-(.12+.86*(1-Burst)))))*step(.001,Burst);
return saturate(max(pool,edge*(.75+.25*Progress))+wall+teeth+shock)*inside;
'''


def param(m, name, value):
    n = e.create_material_expression(m, unreal.MaterialExpressionScalarParameter)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', value)
    return n


def rebuild_material():
    m = unreal.load_asset(ROOT + '/M_GroundBurstWarning')
    sigil = unreal.load_asset(ROOT + '/T_GroundBurstSigil')
    assert m and sigil
    e.delete_all_material_expressions(m)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    uv = e.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate)
    n = e.create_material_expression(m, unreal.MaterialExpressionCustom)
    n.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    n.set_editor_property('code', CODE)
    inputs = []
    for key in ['UV', 'Progress', 'Shape', 'Burst']:
        item = unreal.CustomInput(); item.set_editor_property('input_name', key); inputs.append(item)
    n.set_editor_property('inputs', inputs)
    e.connect_material_expressions(uv, '', n, 'UV')
    for key in ['Progress', 'Shape', 'Burst']:
        e.connect_material_expressions(param(m, key, 0.), '', n, key)
    color = e.create_material_expression(m, unreal.MaterialExpressionVectorParameter)
    color.set_editor_property('parameter_name', 'CueColor')
    color.set_editor_property('default_value', unreal.LinearColor(.30, .018, .008, 1))
    tex = e.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D)
    tex.set_editor_property('parameter_name', 'Sigil'); tex.set_editor_property('texture', sigil)
    gain = e.create_material_expression(m, unreal.MaterialExpressionMultiply)
    e.connect_material_expressions(tex, 'A', gain, 'A')
    e.connect_material_expressions(param(m, 'SigilOpacity', .35), '', gain, 'B')
    opacity = e.create_material_expression(m, unreal.MaterialExpressionMax)
    e.connect_material_expressions(n, '', opacity, 'A'); e.connect_material_expressions(gain, '', opacity, 'B')
    painted = e.create_material_expression(m, unreal.MaterialExpressionMultiply)
    e.connect_material_expressions(tex, 'RGB', painted, 'A')
    e.connect_material_expressions(param(m, 'SigilEmission', .2), '', painted, 'B')
    tint = e.create_material_expression(m, unreal.MaterialExpressionMultiply)
    e.connect_material_expressions(color, '', tint, 'A'); e.connect_material_expressions(n, '', tint, 'B')
    emission = e.create_material_expression(m, unreal.MaterialExpressionAdd)
    e.connect_material_expressions(painted, '', emission, 'A'); e.connect_material_expressions(tint, '', emission, 'B')
    body = e.create_material_expression(m, unreal.MaterialExpressionVectorParameter)
    body.set_editor_property('parameter_name', 'SigilBodyColor')
    body.set_editor_property('default_value', unreal.LinearColor(.012, 0, 0, 1))
    e.connect_material_property(body, '', unreal.MaterialProperty.MP_BASE_COLOR)
    e.connect_material_property(param(m, 'GroundRoughness', 1.), '', unreal.MaterialProperty.MP_ROUGHNESS)
    fade = e.create_material_expression(m, unreal.MaterialExpressionMultiply)
    e.connect_material_expressions(opacity, '', fade, 'A')
    e.connect_material_expressions(param(m, 'RenderOpacity', 1.), '', fade, 'B')
    e.connect_material_property(fade, '', unreal.MaterialProperty.MP_OPACITY)
    e.connect_material_property(emission, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    e.recompile_material(m)
    assert EAL.save_loaded_asset(m, False)
    unreal.log_warning('GROUND_WARNING material rebuilt (oppressive)')


def scale_radii():
    changed = 0
    for path in EAL.list_assets(ATTACK_DIR, recursive=True, include_folder=False):
        asset = EAL.load_asset(path)
        if not isinstance(asset, unreal.AttackDataAsset):
            continue
        markers = list(asset.get_editor_property('impact_markers'))
        touched = False
        for marker in markers:
            if marker.get_editor_property('damage_shape') != unreal.BossAttackDamageShape.GROUND_BURST:
                continue
            gb = marker.get_editor_property('ground_burst')
            old = gb.get_editor_property('radius_cm')
            if abs(old - TARGET_RADIUS) < 1.:
                continue
            gb.set_editor_property('radius_cm', TARGET_RADIUS)
            marker.set_editor_property('ground_burst', gb)
            touched = True
            unreal.log_warning('GROUND_WARNING %s radius %.0f -> %.0f' % (asset.get_name(), old, TARGET_RADIUS))
        if touched:
            asset.set_editor_property('impact_markers', markers)
            assert EAL.save_loaded_asset(asset, False)
            changed += 1
    unreal.log_warning('GROUND_WARNING radius assets changed=%d' % changed)


rebuild_material()
scale_radii()
