"""UE5.5 editor Python: import the selected CC0 sources; no gameplay Blueprint graphs.

Run with UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>.
Only SourceArt/Kenney selection is imported, never the downloaded full archives.
"""
import unreal as u
import pathlib
import json

ROOT = pathlib.Path(u.Paths.project_dir())
SOURCE = ROOT / 'SourceArt' / 'Kenney'
DEST = '/Game/DefendYourKingdom/Art'
tools = u.AssetToolsHelpers.get_asset_tools()


def import_file(path, folder, name, options=None):
    task = u.AssetImportTask()
    task.filename = str(path.resolve())
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options:
        task.options = options
    tools.import_asset_tasks([task])
    assert task.imported_object_paths, str(path)
    asset = u.load_asset(task.imported_object_paths[0])
    assert asset, str(path)
    return asset


def palette(group):
    texture = import_file(SOURCE / group / 'Textures' / 'colormap.png', DEST + '/' + group, 'T_Palette')
    mat = tools.create_asset('M_Palette', DEST + '/' + group, u.Material, u.MaterialFactoryNew())
    if not mat:
        mat = u.load_asset(DEST + '/' + group + '/M_Palette')
    u.MaterialEditingLibrary.delete_all_material_expressions(mat)
    expr = u.MaterialEditingLibrary.create_material_expression(mat, u.MaterialExpressionTextureSample)
    expr.texture = texture
    u.MaterialEditingLibrary.connect_material_property(expr, 'RGB', u.MaterialProperty.MP_BASE_COLOR)
    u.MaterialEditingLibrary.recompile_material(mat)
    u.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


inventory = []
for group in ['Town', 'Dungeon']:
    material = palette(group)
    for path in sorted((SOURCE / group).glob('*.fbx')):
        name = 'SM_' + path.stem.replace('-', '_')
        options = u.FbxImportUI()
        options.automated_import_should_detect_type = False
        options.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
        options.import_as_skeletal = False
        options.import_materials = False
        options.import_textures = False
        options.static_mesh_import_data.combine_meshes = True
        options.static_mesh_import_data.generate_lightmap_u_vs = False
        mesh = import_file(path, DEST + '/' + group, name, options)
        assert isinstance(mesh, u.StaticMesh), name
        for i in range(max(1, len(mesh.static_materials))):
            mesh.set_material(i, material)
        u.EditorAssetLibrary.save_loaded_asset(mesh)
        box = mesh.get_bounds()
        inventory.append({'asset': mesh.get_path_name(), 'origin': str(box.origin), 'extent': str(box.box_extent)})

for path in sorted((SOURCE / 'UI').glob('*.png')):
    tex = import_file(path, DEST + '/UI', path.stem)
    tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_UI)
    u.EditorAssetLibrary.save_loaded_asset(tex)

for name, parent in [('WBP_Start', u.DYKStartWidget), ('WBP_City', u.DYKCityResourceWidget), ('WBP_Battle', u.DYKPlacementWidget)]:
    asset = u.load_asset('/Game/DefendYourKingdom/UI/' + name)
    if not asset:
        factory = u.WidgetBlueprintFactory()
        factory.set_editor_property('parent_class', parent)
        asset = tools.create_asset(name, '/Game/DefendYourKingdom/UI', u.WidgetBlueprint, factory)
    defaults = u.get_default_object(asset.generated_class())
    defaults.set_editor_property('panel_texture', u.load_asset(DEST + '/UI/panel_beige'))
    defaults.set_editor_property('button_texture', u.load_asset(DEST + '/UI/buttonLong_beige'))
    u.BlueprintEditorLibrary.compile_blueprint(asset)
    u.EditorAssetLibrary.save_loaded_asset(asset)

(ROOT / 'Saved/M110/import-inventory.json').write_text(json.dumps(inventory, indent=2))
u.log('M110_IMPORT_OK')
