"""Explicit PIE-only Enhanced Input audit; no saved gameplay settings are changed."""
import unreal, time, json, math, traceback
from pathlib import Path
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
start=time.monotonic(); phase=0; handle=None; results=[]; case=0; sent=False; chained=False
root=Path('D:/Udev/HearBlade')
def finish(error=None):
    if error: unreal.log_error(error)
    (root/'output/arena/backward_dodge_pie.json').write_text(json.dumps({'results':results,'error':error},indent=2))
    editor.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,start,pc,pawn,combat,inputs,action,case,sent,chained,origin,back,pressed
    try:
        elapsed=time.monotonic()-start
        if phase==0 and elapsed>8:
            editor.editor_request_begin_play();phase=1;start=time.monotonic()
        elif phase==1 and editor.is_in_play_in_editor() and elapsed>3:
            world=unreal.EditorLevelLibrary.get_game_world()
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            pawn=unreal.GameplayStatics.get_player_character(world,0);combat=pawn.get_component_by_class(unreal.PlayerRhythmCombatComponent)
            for ai in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.AIController):
                brain=ai.get_editor_property('brain_component')
                if brain: brain.stop_logic('Temporary dodge input audit')
            camera=pawn.get_component_by_class(unreal.BossCombatCameraComponent)
            camera.clear_combat_target();camera.set_component_tick_enabled(False)
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor):
                if actor.get_component_by_class(unreal.BeatSyncedComboComponent): actor.destroy_actor()
            inputs=next(s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if 'Default__' not in s.get_path_name())
            action=pawn.get_editor_property('move_action')
            phase=2;start=time.monotonic()
        elif phase==2:
            yaw=[0,90,180][case]
            pc.set_control_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0))
            pawn.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
            back=unreal.Vector(-math.cos(math.radians(yaw)),-math.sin(math.radians(yaw)),0)
            if case==1: combat.try_block_start()
            inputs.inject_input_vector_for_action(action,unreal.Vector(0,-1,0),[],[])
            sent=False;chained=False;phase=3;start=time.monotonic()
        elif phase==3:
            inputs.inject_input_vector_for_action(action,unreal.Vector(0,-1,0),[],[])
            if elapsed>0.15 and not sent:
                unreal.log_warning('DODGE_INPUT_ROTATION '+str(pc.get_control_rotation()))
                origin=pawn.get_actor_location();pawn.do_dodge();sent=True;pressed=time.monotonic()
            if sent and case==2 and not chained and time.monotonic()-pressed>0.06:
                pawn.do_dodge();chained=True
            if sent and time.monotonic()-pressed>0.48:
                delta=pawn.get_actor_location()-origin
                projection=delta.x*back.x+delta.y*back.y
                result={'case':['neutral S','block to S dodge','S chained dodge'][case],'yaw':[0,90,180][case],'backward_distance_cm':projection,'passed':projection>30}
                results.append(result);unreal.log_warning('BACKWARD_DODGE_AUDIT '+str(result))
                combat.end_block();phase=4;start=time.monotonic()
        elif phase==4 and elapsed>1.2:
            case+=1
            if case==3: finish()
            else: phase=2
        if elapsed>35: finish('timeout phase '+str(phase))
    except Exception:
        finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
