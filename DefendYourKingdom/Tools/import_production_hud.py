"""Import the shared production windows and the compact military popup."""
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
          ('InteriorBase','OrderNormal','OrderDisabled')}
producers={'CITY-MILL':'Bakery','CITY-SAWMILL':'Sawmill','CITY-QUARRY':'Quarry','CITY-FORGE':'Forge'}
recipe_positions=(235,497,757,1017)
recipe_illustrations={}
for building,name in producers.items():
 recipe_images=u.DYKProductionRecipeImages()
 recipe_images.set_editor_property('images',[
  import_ui_texture(f'{name}_{index:02}',source/'RecipeArt',dest+'/RecipeArt')
  for index in range(1,len(recipe_positions)+1)
 ])
 recipe_illustrations[building]=recipe_images
def definition(name):
 folder='/Game/DefendYourKingdom/Art/ResourceHUD'
 path=folder+'/'+name
 if u.EditorAssetLibrary.does_asset_exist(path):
  return u.load_asset(path)
 factory=u.DataAssetFactory();factory.set_editor_property('data_asset_class',u.load_class(None,'/Script/DefendYourKingdom.DYKResourceHUDDefinition'))
 return u.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,None,factory)

asset=definition('ProductionTemplate')
asset.set_editor_property('building_id','None');asset.set_editor_property('production_template',True);asset.set_editor_property('first_only',False)
asset.set_editor_property('first_size',u.Vector2D(1206,1056));asset.set_editor_property('interior_size',u.Vector2D(1448,1086));asset.set_editor_property('resource_icon',None)
recipe_slots=[]
for x in recipe_positions:
 slot=u.DYKProductionIllustrationSlot()
 slot.set_editor_property('position',u.Vector2D(x,268))
 slot.set_editor_property('size',u.Vector2D(218,170))
 recipe_slots.append(slot)
asset.set_editor_property('interior_illustration_slots',recipe_slots)
asset.set_editor_property('interior_illustrations',recipe_illustrations)
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

def compact_texture(name):
 return import_ui_texture(name,source/'Compact',dest+'/Compact')

def popup_parts(kind,width):
 part('FirstBase',(0,0,width,1056),texture=compact_texture(kind+'Base'))
 part('CloseFirst',(1033,105,139,134),action=u.DYKResourceAction.CLOSE,outline=ellipse(1102,172,62,62))
 for key,rect,action,corner,has_gray in [
  ('Collect',(227,412,753,134),u.DYKResourceAction.COLLECT,(40,33),True),
  ('Visit',(216,555,779,127),u.DYKResourceAction.VISIT,(36,32),False),
  ('Upgrade',(174,701,422,88),u.DYKResourceAction.UPGRADE,(23,22),True),
  ('CancelWork',(613,701,422,88),u.DYKResourceAction.CANCEL_WORK,(23,22),True),
  ('Rotate',(179,803,416,92),u.DYKResourceAction.ROTATE,(22,22),False),
  ('Move',(613,803,417,92),u.DYKResourceAction.MOVE,(22,22),False),
 ]:
  x,y,w,h=rect
  part(key,rect,action=action,outline=bevel(x+3,y+3,x+w-5,y+h-5,*corner),
       disabled=compact_texture(kind+key+'Disabled') if has_gray else None)

popup_parts('Production',1206)
part('InteriorBase',(0,0,1448,1086),True,texture=textures['InteriorBase'])
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
def popup_labels():
 label('BUILDING_NAME',(335,146,540,83),56)
 label('WORK_TIME',(808,317,210,60),33,color=yellow)

popup_labels()
label('STORED',(366,317,105,60),36,color=yellow)
label('QUEUE',(665,317,92,60),36,color=yellow)
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

military=definition('MilitaryPopup')
military.set_editor_property('production_template',False)
military.set_editor_property('first_only',True)
military.set_editor_property('first_size',u.Vector2D(1205,1056))
parts.clear()
popup_parts('Military',1205)
military.set_editor_property('parts',parts)
slots.clear()
popup_labels()
label('WORK_NAME',(201,317,561,60),39)
military.set_editor_property('text_slots',slots)
if not u.EditorAssetLibrary.save_loaded_asset(military):
 raise RuntimeError('Could not save MilitaryPopup')
print('PRODUCTION_TEMPLATE_IMPORTED: compact production and military popups')
