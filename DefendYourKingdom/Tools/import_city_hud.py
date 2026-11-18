"""Import the author's HUD art without changing the source PNGs or opening a game."""
import json
from pathlib import Path
import unreal as u

root = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).parent
source = root / 'concept-arts' / 'HUD'
destination = '/Game/DefendYourKingdom/Art/CityHUD'
names = ['WorkshopHUD', 'AllTrapHUD', 'BarraksHUD', 'AllUnitHUD', 'LibraryHUD',
         'SawmillHUD', 'MillHUD', 'QuarryHUD', 'ForgeHUD']
report = []
for name in names:
    filename = source / (name + '.png')
    if not filename.is_file():
        raise RuntimeError('Missing author HUD: ' + str(filename))
    task = u.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = u.load_asset(destination + '/' + name)
    if not isinstance(texture, u.Texture2D):
        raise RuntimeError('HUD texture import failed: ' + name)
    texture.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('srgb', True)
    u.EditorAssetLibrary.save_loaded_asset(texture)
    report.append({'source': str(filename), 'asset': texture.get_path_name()})
out = root / 'DefendYourKingdom' / 'Saved' / 'CityHUD'
out.mkdir(parents=True, exist_ok=True)
(out / 'import-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
print('CITY_HUD_IMPORTED ' + str(len(report)))
