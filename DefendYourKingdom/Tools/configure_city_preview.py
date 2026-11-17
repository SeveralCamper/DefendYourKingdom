"""UE5.5 editor commandlet: generate the shared textured city placement material.

This is presentation only. No city data or map is changed.
"""
import unreal as u

folder = '/Game/DefendYourKingdom/Materials'
name = 'M_CityPlacementPreview'
tools = u.AssetToolsHelpers.get_asset_tools()
material = u.load_asset(folder + '/' + name)
if material is None:
    material = tools.create_asset(name, folder, u.Material, u.MaterialFactoryNew())
assert material
material.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property('two_sided', True)
u.MaterialEditingLibrary.delete_all_material_expressions(material)
palette = u.MaterialEditingLibrary.create_material_expression(material, u.MaterialExpressionTextureSampleParameter2D)
palette.set_editor_property('parameter_name', 'PaletteTexture')
palette.set_editor_property('texture', u.load_asset('/Game/DefendYourKingdom/Art/Town/T_Palette'))
opacity = u.MaterialEditingLibrary.create_material_expression(material, u.MaterialExpressionScalarParameter)
opacity.set_editor_property('parameter_name', 'Opacity')
opacity.set_editor_property('default_value', .7)
assert u.MaterialEditingLibrary.connect_material_property(palette, 'RGB', u.MaterialProperty.MP_BASE_COLOR)
assert u.MaterialEditingLibrary.connect_material_property(opacity, '', u.MaterialProperty.MP_OPACITY)
u.MaterialEditingLibrary.recompile_material(material)
assert u.EditorAssetLibrary.save_loaded_asset(material)
# Roads and residents use ISM components. Save the required permutations explicitly;
# automatic editor-only usage discovery must not leave Game running the default material.
for group in ('Town', 'Dungeon'):
    instanced_material = u.load_asset('/Game/DefendYourKingdom/Art/' + group + '/M_Palette')
    assert instanced_material
    u.MaterialEditingLibrary.set_material_usage(instanced_material, u.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    u.MaterialEditingLibrary.recompile_material(instanced_material)
    assert u.EditorAssetLibrary.save_loaded_asset(instanced_material)
    assert instanced_material.get_editor_property('used_with_instanced_static_meshes')
u.log('CITY_PLACEMENT_PREVIEW_MATERIAL_OK')
