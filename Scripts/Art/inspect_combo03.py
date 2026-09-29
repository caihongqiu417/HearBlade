import unreal,json
p='/Game/SwordAnim/Animation/UE5/Combo03'
a=unreal.load_asset(p)
out={'asset':p,'type':a.get_class().get_name()}
for prop in ['skeleton','sequence_length','sampling_frame_rate','number_of_sampled_keys','enable_root_motion']:
 try:out[prop]=str(a.get_editor_property(prop))
 except Exception as e:out[prop]=str(e)
unreal.log_warning('COMBO03_METADATA '+json.dumps(out))
for name in ['get_frame_rate','get_num_frames','get_sequence_length']:
 try:unreal.log_warning(name+' '+str(getattr(unreal.AnimationLibrary,name)(a)))
 except Exception as e:unreal.log_warning(name+' '+str(e))
