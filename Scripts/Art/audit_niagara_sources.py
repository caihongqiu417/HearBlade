import unreal
for path in ['/Game/Enemies/DF_DRAGON_KNIGHT/VFX/NS_Embers_System','/Game/Combat/Feedback/NS_PerfectDodge_Ring','/Game/Enemies/DF_DRAGON_KNIGHT/VFX/NS_Fire_local_System']:
 a=unreal.load_asset(path)
 t=unreal.AssetExportTask();t.object=a;t.filename='D:/Udev/HearBlade/output/arena/'+a.get_name()+'.copy';t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D()
 unreal.log_warning('EXPORTED '+str(unreal.Exporter.run_asset_export_task(t)))
