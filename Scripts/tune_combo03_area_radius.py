"""Change only Combo03 ground radius, preserving authored timings and responses."""
import unreal
path='/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo03_Beat'
data=unreal.load_asset(path)
assert data, path
markers=list(data.get_editor_property('impact_markers'))
count=0
for marker in markers:
    if marker.get_editor_property('damage_shape')==unreal.BossAttackDamageShape.GROUND_BURST:
        settings=marker.get_editor_property('ground_burst')
        old=settings.get_editor_property('radius_cm')
        settings.set_editor_property('radius_cm',600.)
        marker.set_editor_property('ground_burst',settings)
        count+=1
        unreal.log_warning('COMBO03_AREA_RADIUS %.1f -> 600.0cm'%old)
assert count==1, 'Expected exactly one area impact'
data.set_editor_property('impact_markers',markers)
assert unreal.EditorAssetLibrary.save_loaded_asset(data), 'Could not save Combo03 radius'
