"""Import literal reference slices and configure the reusable resource-building HUD."""
import json
from pathlib import Path
import unreal as u
root = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
source = root / 'SourceArt/CityHUD/Bakery'
dest = '/Game/DefendYourKingdom/Art/ResourceHUD'
textures = {}
rows = json.loads((source / 'slices.json').read_text(encoding='utf-8-sig'))
def texture_name(key):
    if key == 'Statistics': return 'StatisticsLive'
    if key == 'Collect': return 'CollectPlate'
    if key.startswith('Offer'): return 'OfferLive' + key[5:]
    if key.startswith('Order'): return 'Order0'
    return key
used = {'FirstBase', 'InteriorBase', 'BreadIcon'} | {texture_name(row['name']) for row in rows}
for name in sorted(used):
    path = source / 'Parts' / (name + '.png')
    task = u.AssetImportTask()
    task.filename = str(path)
    task.destination_path = dest + '/Bakery'
    task.destination_name = path.stem
    task.automated = True
    task.replace_existing = True
    task.save = False
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = u.load_asset(task.destination_path + '/' + path.stem)
    texture.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('srgb', True)
    u.EditorAssetLibrary.save_loaded_asset(texture)
    textures[path.stem] = texture

def data_asset(name, class_name):
    path = dest + '/' + name
    if u.EditorAssetLibrary.does_asset_exist(path):
        return u.load_asset(path)
    cls = u.load_class(None, '/Script/DefendYourKingdom.' + class_name)
    factory = u.DataAssetFactory()
    factory.set_editor_property('data_asset_class', cls)
    return u.AssetToolsHelpers.get_asset_tools().create_asset(name, dest, cls, factory)

definition = data_asset('Bakery', 'DYKResourceHUDDefinition')
definition.set_editor_property('building_id', 'CITY-MILL')  # stable save identity, displayed as Bakery
parts = []
def part(key, texture, rect, inside=False, action='None', index=0):
    row = u.DYKResourceHUDPart()
    row.set_editor_property('key', key)
    row.set_editor_property('texture', textures[texture])
    row.set_editor_property('position', u.Vector2D(rect[0], rect[1]))
    row.set_editor_property('size', u.Vector2D(rect[2], rect[3]))
    row.set_editor_property('interior', inside)
    row.set_editor_property('action', getattr(u.DYKResourceAction, 'NONE' if action == 'None' else ''.join(('_' + c if c.isupper() else c) for c in action).lstrip('_').upper()))
    row.set_editor_property('index', index)
    parts.append(row)
part('FirstBase', 'FirstBase', [0,0,1122,1402])
part('InteriorBase', 'InteriorBase', [0,0,1448,1086], True)
for row in rows:
    key = row['name']
    texture = texture_name(key)
    part(key, texture, row['rect'], row['inside'], row['action'], row['index'])
definition.set_editor_property('parts', parts)
definition.set_editor_property('resource_icon', textures['BreadIcon'])
u.EditorAssetLibrary.save_loaded_asset(definition)
catalog = data_asset('Catalog', 'DYKResourceHUDCatalog')
catalog.set_editor_property('definitions', [definition])
u.EditorAssetLibrary.save_loaded_asset(catalog)
print('BAKERY_HUD_IMPORTED', len(textures), 'LITERAL_REFERENCE_SLICES')
