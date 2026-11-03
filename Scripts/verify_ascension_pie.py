"""Two PIE sessions: phase fallback plus camera/input recovery; never save map changes."""
import unreal,time,json,traceback
from pathlib import Path
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
stage=0;start=time.monotonic();round_index=0;results=[];handle=None;phase_seen=False;transition_seen=False
def finish(error=None):
    Path('D:/Udev/HearBlade/output/arena/ascension_pie.json').write_text(json.dumps({'results':results,'error':error},indent=2))
    if error:unreal.log_error(error)
    unreal.unregister_slate_post_tick_callback(handle)
    editor.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()
def tick(dt):
    global stage,start,round_index,phase_seen,transition_seen
    try:
        elapsed=time.monotonic()-start
        if stage==0 and elapsed>8:
            editor.editor_request_begin_play();stage=1;start=time.monotonic();phase_seen=False;transition_seen=False
        elif stage==1 and editor.is_in_play_in_editor():
            stage=2;start=time.monotonic();unreal.log_warning('ASCENSION_PIE_START '+str(round_index))
        elif stage==2:
            world=unreal.EditorLevelLibrary.get_game_world()
            pawn=unreal.GameplayStatics.get_player_character(world,0)
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            if not pawn:return
            bosses=[a.get_component_by_class(unreal.BeatSyncedComboComponent) for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)]
            boss=next((b for b in bosses if b),None)
            if not boss:return
            transition_seen |= boss.is_phase_transition_active()
            if boss.get_editor_property('boss_phase')==2 and not boss.is_phase_transition_active():
                if not phase_seen:phase_seen=True;start=time.monotonic();return
                if elapsed>1.0:
                    combat=pawn.get_component_by_class(unreal.PlayerDefenseComponent)
                    camera_ok=pc.get_view_target()==pawn
                    for ai in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.AIController):
                        brain=ai.get_editor_property('brain_component')
                        if brain:brain.stop_logic('PIE recovery probe')
                    action_ok=combat.request_dodge(pawn.get_actor_right_vector())
                    row={'round':round_index,'transition_seen':transition_seen,'phase':2,'camera_returned':camera_ok,'dodge_accepted':action_ok}
                    results.append(row);unreal.log_warning('ASCENSION_PIE_RESULT '+str(row))
                    assert transition_seen and camera_ok and action_ok,row
                    editor.editor_request_end_play();stage=3;start=time.monotonic()
            elif elapsed>55:raise RuntimeError('Phase transition timeout')
        elif stage==3 and elapsed>4 and not editor.is_in_play_in_editor():
            round_index+=1
            if round_index>=2:finish()
            else:stage=0;start=time.monotonic()
    except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
