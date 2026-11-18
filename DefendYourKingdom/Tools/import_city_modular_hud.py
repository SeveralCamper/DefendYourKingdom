"""Import modular HUD sources and an editable, presentation-only DataAsset. No play or render."""
import hashlib
import json
from pathlib import Path
import unreal as u

root = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).parent
sources = root / 'DefendYourKingdom/SourceArt/CityHUD'
destination = '/Game/DefendYourKingdom/Art/CityHUD'
assets = {}
report = []
font_only = '-CityFontOnly' in u.SystemLibrary.get_command_line()

def import_file(filename, name, folder=destination, factory=None):
    task = u.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    if factory:
        task.factory = factory
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = u.load_asset(folder + '/' + name)
    if not asset:
        raise RuntimeError('Import failed: ' + name)
    if isinstance(asset, u.Texture2D):
        asset.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_EDITOR_ICON)
        asset.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_UI)
        asset.set_editor_property('mip_gen_settings', u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        asset.set_editor_property('srgb', True)
    u.EditorAssetLibrary.save_loaded_asset(asset)
    report.append({'source': str(filename.relative_to(root)), 'asset': asset.get_path_name(),
                   'sha256': hashlib.sha256(filename.read_bytes()).hexdigest()})
    return asset

for name in ['WindowBase', 'CardFrame', 'NewRibbon', 'TrapPictures', 'UnitPictures', 'ButtonPlate']:
    assets[name] = u.load_asset(destination + '/' + name) if font_only else import_file(sources / 'Modular' / (name + '.png'), name)
font = u.load_asset(destination + '/Fonts/CitySerif') if font_only else import_file(sources / 'Fonts/NotoSerif-Regular.ttf', 'CitySerif', destination + '/Fonts', u.FontFileImportFactory())
if not isinstance(font, u.FontFace):
    raise RuntimeError('Expected self-contained FontFace, got ' + str(font))
font.set_editor_property('loading_policy', u.FontLoadingPolicy.INLINE)
u.EditorAssetLibrary.save_loaded_asset(font)

# A bare FontFace is not an IFontProviderInterface. UMG must receive a runtime UFont.
runtime_font = u.load_asset(destination + '/Fonts/CitySerifFont') if u.EditorAssetLibrary.does_asset_exist(destination + '/Fonts/CitySerifFont') else None
if not runtime_font:
    runtime_font = u.AssetToolsHelpers.get_asset_tools().create_asset('CitySerifFont', destination + '/Fonts', u.Font, u.FontFactory())
if not u.DYKCityPresentation.configure_runtime_font(runtime_font, font):
    raise RuntimeError('Runtime composite font configuration failed')
u.EditorAssetLibrary.save_loaded_asset(runtime_font)

cls = u.load_class(None, '/Script/DefendYourKingdom.DYKCityPresentation')
presentation = u.load_asset(destination + '/CityPresentation')
if not presentation:
    factory = u.DataAssetFactory()
    factory.set_editor_property('data_asset_class', cls)
    presentation = u.AssetToolsHelpers.get_asset_tools().create_asset('CityPresentation', destination, cls, factory)
if not font_only:
    for prop, name in [('window', 'WindowBase'), ('card_frame', 'CardFrame'), ('new_ribbon', 'NewRibbon'), ('button_plate', 'ButtonPlate')]:
        presentation.set_editor_property(prop, assets[name])
presentation.set_editor_property('typeface', runtime_font)

# Reimport intentionally preserves manually configured card pictures on an existing asset.
if not presentation.get_editor_property('pictures'):
    pictures = []
    def picture(key, texture, minimum, maximum):
        row = u.DYKCityCardPicture()
        row.set_editor_property('key', key)
        row.set_editor_property('illustration', texture)
        row.set_editor_property('uv_min', u.Vector2D(*minimum))
        row.set_editor_property('uv_max', u.Vector2D(*maximum))
        pictures.append(row)
    for i in range(4):
        x, y = i % 2, i // 2
        picture('TRAP-CARD-' + str(i), assets['TrapPictures'], (x*.5, y*.5), ((x+1)*.5, (y+1)*.5))
    # Every item has its own row and can point to an entirely different Texture2D.
    for i, quadrant in enumerate([0, 1, 1, 2, 2, 3]):
        x, y = quadrant % 2, quadrant // 2
        picture('UNIT-CARD-' + str(i), assets['UnitPictures'], (x*.5, y*.5), ((x+1)*.5, (y+1)*.5))
    presentation.set_editor_property('pictures', pictures)
u.EditorAssetLibrary.save_loaded_asset(presentation)
out = root / 'DefendYourKingdom/Saved/CityHUDModular'
out.mkdir(parents=True, exist_ok=True)
(out / 'import-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
print('CITY_MODULAR_HUD_IMPORTED ' + str(len(report)) + ' COMPOSITE_FONT_READY')
