import unreal
for name in ['P_Hit','P_Spark']:
 a=unreal.load_asset('/Game/SwordAnim/demo/FX/'+name)
 t=unreal.AssetExportTask();t.object=a;t.filename='D:/Udev/HearBlade/output/arena/'+name+'.copy';t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D()
 unreal.log_warning('EXPORTED '+str(unreal.Exporter.run_asset_export_task(t)))
