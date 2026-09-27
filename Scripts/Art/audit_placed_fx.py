import unreal
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/ThirdPerson/Lvl_ThirdPerson')
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 for c in a.get_components_by_class(unreal.NiagaraComponent):
  unreal.log_warning('PLACED_FX '+a.get_actor_label()+' '+c.get_path_name()+' '+str(c.get_asset())+' '+str(c.get_world_location()))
