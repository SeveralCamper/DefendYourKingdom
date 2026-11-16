"""Reproducible visual dressing of existing technical maps; routes/slots/rules preserved."""
import unreal as u
import pathlib
import json
ROOT = pathlib.Path(u.Paths.project_dir())
BASE = '/Game/DefendYourKingdom'
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
tools = u.AssetToolsHelpers.get_asset_tools()

def spawn(group, name, position, scale=(1,1,1), yaw=0):
    actor = actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(*position),u.Rotator(0,yaw,0))
    actor.set_actor_label('M110_' + name)
    actor.tags = ['DEV_PLACEHOLDER','M110_VISUAL']
    actor.static_mesh_component.set_static_mesh(u.load_asset(f'{BASE}/Art/{group}/SM_{name}'))
    actor.static_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    actor.set_actor_scale3d(u.Vector(*scale))
    return actor

def flat_material(name,color):
    path=f'{BASE}/Art/{name}'
    mat = u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,BASE+'/Art',u.Material,u.MaterialFactoryNew())
    u.MaterialEditingLibrary.delete_all_material_expressions(mat)
    c=u.MaterialEditingLibrary.create_material_expression(mat,u.MaterialExpressionConstant3Vector)
    c.constant=u.LinearColor(*color,1)
    u.MaterialEditingLibrary.connect_material_property(c,'',u.MaterialProperty.MP_BASE_COLOR)
    u.MaterialEditingLibrary.recompile_material(mat)
    u.EditorAssetLibrary.save_loaded_asset(mat)
    return mat

grass = flat_material('M_Grass',(0.12,0.23,0.09))
inventory={}
for name in ['L_Citadel','L_MissionPolygon']:
    assert levels.load_level(f'{BASE}/Maps/{name}')
    for a in list(actors.get_all_level_actors()):
        if 'M110_VISUAL' in [str(t) for t in a.tags]: actors.destroy_actor(a)
    for a in actors.get_all_level_actors():
        label=a.get_actor_label()
        if isinstance(a,u.TextRenderActor): a.set_actor_hidden_in_game(True); a.set_actor_enable_collision(False)
        if a.get_class().get_name() == 'DYKSelectionTargetActor': a.set_actor_hidden_in_game(True); a.set_actor_enable_collision(False)
        if isinstance(a,u.StaticMeshActor):
            if label == 'Draft_Ground' or name == 'L_Citadel': a.static_mesh_component.set_material(0,grass)
            if label.startswith('Draft_Road'):
                a.static_mesh_component.set_static_mesh(u.load_asset(BASE+'/Art/Town/SM_road'))
                a.static_mesh_component.set_material(0,u.load_asset(BASE+'/Art/Town/M_Palette'))
                old=a.get_actor_scale3d(); a.set_actor_scale3d(u.Vector(old.x,old.y,1))
                a.static_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
            if label.startswith('Draft_Tower') or label in ['Draft_Spawn_Landmark','Draft_Defended_Landmark']: a.set_actor_hidden_in_game(True); a.set_actor_enable_collision(False)
        if isinstance(a,u.DirectionalLight): a.light_component.set_editor_property('intensity',3.0)
    if name == 'L_Citadel':
        for t in range(-5,6):
            spawn('Town','road',(t*400,0,3),(4,4,1))
            if t: spawn('Town','road',(0,t*400,3),(4,4,1))
        for x,y in [(-2700,-2600),(-2200,-2900),(-1400,-2700),(1000,-2900),(2200,-2700),(2800,-1700),(2700,0),(2700,1600),(2300,2600),(1200,2700),(-900,2800),(-2200,2600),(-2700,1400),(-2800,0)]:
            spawn('Town','tree_high',(x,y,0),(2,2,2))
        spawn('Town','planks',(-1800,0,2),(5,5,1))
        spawn('Dungeon','rocks',(1800,250,0),(4,3,3))
    else:
        # Castle is the protected landmark; gameplay objective IDs remain in the battle definition.
        spawn('Town','wall_block',(3900,700,0),(4,4,3))
        for x in [-170,170]:
            for y in [-170,170]:
                spawn('Town','wall_block',(3900+x,700+y,0),(1.4,1.4,4.5))
                spawn('Town','roof_high_point',(3900+x,700+y,450),(1.5,1.5,1.5))
        spawn('Dungeon','wood_structure',(-4200,-1300,0),(3,4,3))
        for x,y in [(-4400,-2000),(-3500,-2300),(-2700,-2200),(-1400,-2300),(-600,-2100),(700,-1800),(1800,-1400),(3000,-1400),(4400,-800),(4400,2000),(3200,2300),(2200,2300),(900,2400),(-300,2400),(-1400,2100),(-2600,1700),(-3700,1400),(-4500,700)]:
            spawn('Town','tree',(x,y,0),(2.5,2.5,2.5))
        spawn('Dungeon','rocks',(-900,1600,0),(5,3,3))
        spawn('Dungeon','rocks',(1600,-300,0),(3,4,3))
    assert levels.save_current_level()
    inventory[name] = [{'label':a.get_actor_label(),'class':a.get_class().get_name(),'location':str(a.get_actor_location())} for a in actors.get_all_level_actors()]
(ROOT/'Saved/M110/map-inventory.json').write_text(json.dumps(inventory,indent=2))
u.log('M110_SCENES_OK')
