import unreal,time,json,traceback
from pathlib import Path
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
stage=0;start=time.monotonic();handle=None;seen=False
def done(result):
    Path('D:/Udev/HearBlade/output/arena/ascension_abort_pie.json').write_text(json.dumps(result,indent=2))
    unreal.log_warning('ASCENSION_ABORT_RESULT '+str(result))
    editor.editor_request_end_play();unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global stage,start,seen
    try:
        elapsed=time.monotonic()-start
        if stage==0 and elapsed>8:editor.editor_request_begin_play();stage=1;start=time.monotonic()
        elif stage==1 and editor.is_in_play_in_editor():stage=2;start=time.monotonic()
        elif stage==2:
            world=unreal.EditorLevelLibrary.get_game_world()
            bosses=[a.get_component_by_class(unreal.BeatSyncedComboComponent) for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)]
            boss=next((b for b in bosses if b),None)
            if boss:seen |= boss.is_phase_transition_active()
            if seen and not boss:
                pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_character(world,0)
                combat=pawn.get_component_by_class(unreal.PlayerDefenseComponent)
                result={'transition_seen':seen,'target_destroyed':True,'camera_returned':pc.get_view_target()==pawn,'block_accepted':combat.request_block()}
                assert all(result.values()),str(result)
                done(result)
            elif elapsed>40:raise RuntimeError('Abort test timeout')
    except Exception:done({'error':traceback.format_exc()})
handle=unreal.register_slate_post_tick_callback(tick)
