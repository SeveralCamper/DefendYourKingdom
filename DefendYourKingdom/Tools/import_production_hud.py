"""One immutable two-window production template, with transparent controls and data slots."""
from pathlib import Path
import unreal as u
root=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
source=root/'SourceArt/CityHUD/ProductionTemplate'
dest='/Game/DefendYourKingdom/Art/ResourceHUD/Shared'
textures={}
for name in ('FirstBase','InteriorBase'):
 task=u.AssetImportTask();task.filename=str(source/(name+'.png'));task.destination_path=dest;task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=False
 u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 texture=u.load_asset(dest+'/'+name)
 texture.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_EDITOR_ICON)
 texture.set_editor_property('lod_group',u.TextureGroup.TEXTUREGROUP_UI)
 texture.set_editor_property('mip_gen_settings',u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
 texture.set_editor_property('srgb',True);u.EditorAssetLibrary.save_loaded_asset(texture);textures[name]=texture
path='/Game/DefendYourKingdom/Art/ResourceHUD/ProductionTemplate'
asset=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else None
if not asset:
 factory=u.DataAssetFactory();factory.set_editor_property('data_asset_class',u.load_class(None,'/Script/DefendYourKingdom.DYKResourceHUDDefinition'))
 asset=u.AssetToolsHelpers.get_asset_tools().create_asset('ProductionTemplate','/Game/DefendYourKingdom/Art/ResourceHUD',None,factory)
asset.set_editor_property('building_id','None');asset.set_editor_property('production_template',True);asset.set_editor_property('first_only',False)
asset.set_editor_property('first_size',u.Vector2D(1122,1402));asset.set_editor_property('interior_size',u.Vector2D(1448,1086));asset.set_editor_property('resource_icon',None)
parts=[]
def part(key,rect,inside=False,action=u.DYKResourceAction.NONE,index=0,texture=None):
 row=u.DYKResourceHUDPart();row.set_editor_property('key',key);row.set_editor_property('texture',texture);row.set_editor_property('position',u.Vector2D(*rect[:2]));row.set_editor_property('size',u.Vector2D(*rect[2:]));row.set_editor_property('interior',inside);row.set_editor_property('action',action);row.set_editor_property('index',index);parts.append(row)
part('FirstBase',(0,0,1122,1402),texture=textures['FirstBase']);part('InteriorBase',(0,0,1448,1086),True,texture=textures['InteriorBase'])
for key,rect,action in [
 ('CloseFirst',(966,84,143,145),u.DYKResourceAction.CLOSE),
 ('Collect',(220,780,684,120),u.DYKResourceAction.COLLECT),
 ('Visit',(205,913,714,124),u.DYKResourceAction.VISIT),
 ('Upgrade',(167,1051,386,91),u.DYKResourceAction.UPGRADE),
 ('CancelWork',(567,1051,391,91),u.DYKResourceAction.CANCEL_WORK),
 ('Rotate',(179,1152,376,88),u.DYKResourceAction.ROTATE),
 ('Move',(567,1152,391,88),u.DYKResourceAction.MOVE)]:part(key,rect,action=action)
part('CloseInterior',(1256,78,108,100),True,u.DYKResourceAction.CLOSE)
part('Back',(1265,180,96,88),True,u.DYKResourceAction.BACK)
for i in range(4):part('Order'+str(i),(236+260*i,648,223,78),True,u.DYKResourceAction.ORDER,i)
asset.set_editor_property('parts',parts)
slots=[]
cream=(1,.91,.70,1);yellow=(1,.82,.04,1);ink=(.18,.085,.025,1);green=(.4,.8,.025,1)
def label(field,rect,font=28,inside=False,index=0,color=cream):
 slot=u.DYKProductionTextSlot();slot.set_editor_property('field',getattr(u.DYKProductionField,field));slot.set_editor_property('position',u.Vector2D(*rect[:2]));slot.set_editor_property('size',u.Vector2D(*rect[2:]));slot.set_editor_property('font_size',font);slot.set_editor_property('interior',inside);slot.set_editor_property('index',index);slot.set_editor_property('color',u.LinearColor(*color));slots.append(slot)
label('BUILDING_NAME',(325,135,490,90),64)
label('DESCRIPTION',(235,570,650,76),28,color=ink)
label('STORED',(341,688,110,58),34,color=yellow)
label('QUEUE',(629,688,90,58),32,color=yellow)
label('WORK_TIME',(759,688,197,58),30,color=yellow)
label('BUILDING_NAME',(420,102,600,88),54,True)
for i in range(4):
 label('PRODUCT_NAME',(240+260*i,445,220,46),30,True,i)
 label('RECIPE_AMOUNT',(260+260*i,518,180,48),34,True,i)
 label('RECIPE_DURATION',(252+260*i,581,194,48),28,True,i)
label('SPEED',(460,815,96,62),34,True,color=green)
label('QUEUE',(938,820,85,55),32,True,color=yellow)
for i in range(3):label('QUEUE_SLOT',(1036+66*i,823,50,48),27,True,i,yellow)
asset.set_editor_property('text_slots',slots);u.EditorAssetLibrary.save_loaded_asset(asset)
print('PRODUCTION_TEMPLATE_IMPORTED: one template, two base textures, no building/resource illustrations')
