"""One immutable two-window production template, with transparent controls and data slots."""
from pathlib import Path
from math import cos, pi, sin
import unreal as u
root=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
source=root/'SourceArt/CityHUD/ProductionTemplate'
dest='/Game/DefendYourKingdom/Art/ResourceHUD/Shared'
def import_ui_texture(name, folder, destination):
 task=u.AssetImportTask();task.filename=str(folder/(name+'.png'));task.destination_path=destination;task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=False
 u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 texture=u.load_asset(destination+'/'+name)
 texture.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_EDITOR_ICON)
 texture.set_editor_property('lod_group',u.TextureGroup.TEXTUREGROUP_UI)
 texture.set_editor_property('mip_gen_settings',u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
 texture.set_editor_property('srgb',True)
 if not u.EditorAssetLibrary.save_loaded_asset(texture):
  raise RuntimeError(f'Could not save {name}')
 return texture

textures={name:import_ui_texture(name,source,dest) for name in
          ('FirstBase','InteriorBase','CollectDisabled','UpgradeDisabled','CancelWorkDisabled','OrderNormal','OrderDisabled')}
illustrations={building:import_ui_texture(name,source/'Illustrations',dest+'/Illustrations')
               for building,name in (
                ('CITY-MILL','BakeryIcon'),('CITY-SAWMILL','SawmillIcon'),
                ('CITY-QUARRY','QuarryIcon'),('CITY-FORGE','ForgeIcon'))}
path='/Game/DefendYourKingdom/Art/ResourceHUD/ProductionTemplate'
asset=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else None
if not asset:
 factory=u.DataAssetFactory();factory.set_editor_property('data_asset_class',u.load_class(None,'/Script/DefendYourKingdom.DYKResourceHUDDefinition'))
 asset=u.AssetToolsHelpers.get_asset_tools().create_asset('ProductionTemplate','/Game/DefendYourKingdom/Art/ResourceHUD',None,factory)
asset.set_editor_property('building_id','None');asset.set_editor_property('production_template',True);asset.set_editor_property('first_only',False)
asset.set_editor_property('first_size',u.Vector2D(1122,1402));asset.set_editor_property('interior_size',u.Vector2D(1448,1086));asset.set_editor_property('resource_icon',None)
asset.set_editor_property('primary_illustrations',illustrations)
# Leave clearance above the stats frame and below the title banner.
asset.set_editor_property('primary_illustration_position',u.Vector2D(277,257))
asset.set_editor_property('primary_illustration_size',u.Vector2D(568,405))
parts=[]
def part(key,rect,inside=False,action=u.DYKResourceAction.NONE,index=0,texture=None,outline=(),disabled=None):
 x,y,w,h=rect
 row=u.DYKResourceHUDPart()
 row.set_editor_property('key',key)
 row.set_editor_property('texture',texture)
 row.set_editor_property('unavailable_texture',disabled)
 row.set_editor_property('position',u.Vector2D(x,y))
 row.set_editor_property('size',u.Vector2D(w,h))
 row.set_editor_property('interior',inside)
 row.set_editor_property('action',action)
 row.set_editor_property('index',index)
 if outline:
  row.set_editor_property('hover_outline',[u.Vector2D((px-x)/w,(py-y)/h) for px,py in outline])
 parts.append(row)

def bevel(left,top,right,bottom,cut_x,cut_y):
 return [(left+cut_x,top),(right-cut_x,top),(right,top+cut_y),(right,bottom-cut_y),
         (right-cut_x,bottom),(left+cut_x,bottom),(left,bottom-cut_y),(left,top+cut_y),(left+cut_x,top)]

def ellipse(cx,cy,rx,ry):
 # Overlap the two antialiased path caps on the frame side of the circle.
 angles=[2*pi*i/64 for i in range(24,91)]
 return [(cx+rx*cos(angle),cy+ry*sin(angle)) for angle in angles]

part('FirstBase',(0,0,1122,1402),texture=textures['FirstBase']);part('InteriorBase',(0,0,1448,1086),True,texture=textures['InteriorBase'])
for key,rect,action,outline in [
 ('CloseFirst',(966,84,143,145),u.DYKResourceAction.CLOSE,ellipse(1035,155,59,59)),
 ('Collect',(210,770,705,141),u.DYKResourceAction.COLLECT,bevel(225,782,898,892,32,27)),
 ('Visit',(205,913,714,124),u.DYKResourceAction.VISIT,bevel(211,918,909,1028,35,27)),
 ('Upgrade',(157,1043,404,107),u.DYKResourceAction.UPGRADE,bevel(173,1055,550,1132,18,17)),
 ('CancelWork',(562,1043,403,107),u.DYKResourceAction.CANCEL_WORK,bevel(575,1055,951,1132,18,17)),
 ('Rotate',(179,1152,376,88),u.DYKResourceAction.ROTATE,bevel(185,1156,545,1232,18,17)),
 ('Move',(567,1152,391,88),u.DYKResourceAction.MOVE,bevel(575,1156,941,1232,18,17))]:
 part(key,rect,action=action,outline=outline,disabled=textures.get(key+'Disabled'))
part('CloseInterior',(1256,78,108,100),True,u.DYKResourceAction.CLOSE,outline=ellipse(1307,128,47,47))
part('Back',(1265,180,96,91),True,u.DYKResourceAction.BACK,outline=ellipse(1309,224,43,42))
for i,x in enumerate((236,498,757,1016)):
 part('Order'+str(i),(x,646,224,69),True,u.DYKResourceAction.ORDER,i,
      texture=textures['OrderNormal'],outline=bevel(x+4,650,x+218,711,16,16),disabled=textures['OrderDisabled'])
asset.set_editor_property('parts',parts)
slots=[]
cream=(1,.91,.70,1);yellow=(1,.82,.04,1);green=(.4,.8,.025,1)
def label(field,rect,font=28,inside=False,index=0,color=cream):
 slot=u.DYKProductionTextSlot();slot.set_editor_property('field',getattr(u.DYKProductionField,field));slot.set_editor_property('position',u.Vector2D(*rect[:2]));slot.set_editor_property('size',u.Vector2D(*rect[2:]));slot.set_editor_property('font_size',font);slot.set_editor_property('interior',inside);slot.set_editor_property('index',index);slot.set_editor_property('color',u.LinearColor(*color));slots.append(slot)
label('BUILDING_NAME',(325,135,490,90),64)
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
asset.set_editor_property('text_slots',slots)
if not u.EditorAssetLibrary.save_loaded_asset(asset):
 raise RuntimeError('Could not save ProductionTemplate')
print('PRODUCTION_TEMPLATE_IMPORTED: one shared template with authored disabled states')
