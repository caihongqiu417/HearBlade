import unreal
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
for asset in registry.get_assets_by_path('/Game', recursive=True):
    if asset.asset_class_path.asset_name in ('SoundWave', 'SoundCue', 'MetaSoundSource'):
        obj = asset.get_asset()
        unreal.log_warning('PLAYER_AUDIO_ASSET ' + obj.get_path_name() + ' duration=' + str(obj.get_editor_property('duration')))
