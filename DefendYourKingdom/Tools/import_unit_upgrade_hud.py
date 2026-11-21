"""Import the authored unit detail artwork as two UI textures."""
from pathlib import Path
import unreal as ue

project = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
source = project / "SourceArt/CityHUD/UnitUpgrade"
destination = "/Game/DefendYourKingdom/Art/CityHUD/UnitUpgrade"

for name in ("UnitUpgradeBase", "UnitUpgradeParts"):
    task = ue.AssetImportTask()
    task.filename = str(source / (name + ".png"))
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = ue.load_asset(destination + "/" + name)
    texture.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", ue.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", ue.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    if not ue.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError(f"Could not save {name}")

print("UNIT_UPGRADE_ART_IMPORTED")
