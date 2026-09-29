# Opens a standalone test session. Only this process lowers the Boss to the phase
# threshold after six seconds. It does not automate player inputs or save assets.
$projectFile = Join-Path (Split-Path $PSScriptRoot -Parent) 'HearBlade.uproject'
$editorFile = 'D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
& $editorFile $projectFile '/Game/ThirdPerson/Lvl_ThirdPerson' '-game' '-windowed' '-ResX=1600' '-ResY=900' '-PhaseTransitionTest'
