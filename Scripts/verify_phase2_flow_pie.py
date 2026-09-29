"""Explicit unattended PIE regression; no level/asset writes."""
import unreal,time
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
stage=0;start=time.monotonic();handle=None
def tick(delta):
    global stage,start
    elapsed=time.monotonic()-start
    if stage==0 and elapsed>8:
        editor.editor_request_begin_play();stage=1;start=time.monotonic()
    elif stage==1 and editor.is_in_play_in_editor():
        unreal.log_warning('PHASE2_FLOW_PIE_STARTED');stage=2;start=time.monotonic()
    elif stage==2 and elapsed>44:
        editor.editor_request_end_play();stage=3;start=time.monotonic()
    elif stage==3 and elapsed>4 and not editor.is_in_play_in_editor():
        unreal.log_warning('PHASE2_FLOW_PIE_ENDED')
        unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
    elif elapsed>80:
        unreal.log_error('PHASE2_FLOW_PIE_TIMEOUT')
        unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
