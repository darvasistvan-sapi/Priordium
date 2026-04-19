"""
fix_missing_assets.py
Removes actors whose Blueprint class does not exist on disk from the persistent level.
Run via Commands/FixMissingAssets.bat — do NOT run while the level is open in the editor.

Usage (from FixMissingAssets.bat):
    UnrealEditor-Cmd.exe <uproject> <map> -run=pythonscript -script=<this file>
"""

import unreal

MISSING_CLASSES = [
    "BP_Human",
]

def main():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        unreal.log_error("fix_missing_assets: Could not get editor world.")
        return

    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    unreal.log(f"fix_missing_assets: Scanning {len(actors)} actors...")

    to_delete = []
    for actor in actors:
        actor_name  = actor.get_name()
        actor_label = actor.get_actor_label()
        cls         = actor.get_class()
        class_name  = cls.get_name() if cls else "(no class)"

        for missing in MISSING_CLASSES:
            if missing in actor_name or missing in class_name:
                unreal.log(f"  -> Marked for deletion: label='{actor_label}'  name='{actor_name}'  class='{class_name}'")
                to_delete.append(actor)
                break

    if not to_delete:
        unreal.log("fix_missing_assets: No matching actors found. Nothing to do.")
        return

    deleted = 0
    for actor in to_delete:
        if unreal.EditorLevelLibrary.destroy_actor(actor):
            deleted += 1
        else:
            unreal.log_warning(f"fix_missing_assets: Could not destroy '{actor.get_name()}'")

    unreal.EditorLoadingAndSavingUtils.save_current_level()
    unreal.log(f"fix_missing_assets: Done. Deleted {deleted}/{len(to_delete)} actors. Level saved.")

main()
