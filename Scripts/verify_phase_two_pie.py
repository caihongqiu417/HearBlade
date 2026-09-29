"""Explicit unattended editor-only audit: run two PIE sessions, then close editor."""
import unreal, time

editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
stage=0
start=time.monotonic()
handle=None
def tick(delta):
    global stage,start,handle
    elapsed=time.monotonic()-start
    if stage in (0,3) and elapsed>12:
        editor.editor_request_begin_play()
        unreal.log_warning('PHASE_TWO_PIE_REQUEST session=%d'%(1 if stage==0 else 2))
        stage+=1;start=time.monotonic()
    elif stage in (1,4) and editor.is_in_play_in_editor():
        unreal.log_warning('PHASE_TWO_PIE_STARTED session=%d'%(1 if stage==1 else 2))
        stage+=1;start=time.monotonic()
    elif stage in (2,5) and elapsed>42:
        editor.editor_request_end_play()
        unreal.log_warning('PHASE_TWO_PIE_END_REQUEST session=%d'%(1 if stage==2 else 2))
        stage+=1;start=time.monotonic()
    elif stage==6 and elapsed>5 and not editor.is_in_play_in_editor():
        unreal.log_warning('PHASE_TWO_PIE_TWO_SESSIONS_ENDED')
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
    elif elapsed>100:
        unreal.log_error('PHASE_TWO_PIE_AUDIT_TIMEOUT stage=%d'%stage)
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
