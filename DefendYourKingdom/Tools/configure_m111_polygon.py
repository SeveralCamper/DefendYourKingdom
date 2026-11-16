"""Keep four authored slots; expose a natural loss through a weak deployment.

Run with UE5.5 -run=pythonscript -script=<this file>. Idempotent; edits only
the DEV_PLACEHOLDER BendRight position, leaving the two transverse rows intact.
"""
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/DefendYourKingdom/Maps/L_MissionPolygon')
slot_class = unreal.load_class(None, '/Script/DefendYourKingdom.DYKUnitSlot')
slots = [a for a in actors.get_all_level_actors() if a.get_class() == slot_class]
assert len(slots) == 4
matches = [a for a in slots if str(a.get_editor_property('slot_id')) == 'BendRight']
assert len(matches) == 1
slot = matches[0]
assert 'DEV_PLACEHOLDER' in [str(tag) for tag in slot.tags]
# Route exits along Y=700. At Y=1000 this slot cannot intercept it with the
# peasant's approved radius (5 * DEV_PLACEHOLDER 30cm), even at its raised Z.
assert slot.set_actor_location(unreal.Vector(-800, 1000, 80), False, False)
assert levels.save_current_level()
unreal.log('M111_BEND_RIGHT_SAVED: four slots, two rows, natural weak-deployment loss')
