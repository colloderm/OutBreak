import json
import os
import re
import traceback

import unreal


MAP_PATH = "/Game/Maps/OutBreak_Exterior"
SOURCE_T3D = r"C:\Users\Admin\.codex\attachments\223e8a8a-5df8-49e9-bce9-0be1c45e702c\pasted-text.txt"
REPORT_PATH = os.path.join(unreal.Paths.project_saved_dir(), "InteriorPCGToStaticReport.json")
EXPECTED_COUNT = 114
LOCATION_TOLERANCE = 0.01
ROTATION_TOLERANCE = 0.01
SCALE_TOLERANCE = 0.0001


def vector_delta(a, b):
    return max(abs(a.x - b.x), abs(a.y - b.y), abs(a.z - b.z))


def rotation_delta(a, b):
    def angle_delta(x, y):
        return abs((x - y + 180.0) % 360.0 - 180.0)

    return max(
        angle_delta(a.pitch, b.pitch),
        angle_delta(a.yaw, b.yaw),
        angle_delta(a.roll, b.roll),
    )


def get_component_material_paths(component):
    result = []
    for index in range(component.get_num_materials()):
        material = component.get_material(index)
        result.append(material.get_path_name() if material else None)
    return result


def copy_editor_property_if_available(source, destination, property_name):
    try:
        destination.set_editor_property(property_name, source.get_editor_property(property_name))
    except Exception:
        pass


def main():
    report = {
        "map": MAP_PATH,
        "expected_count": EXPECTED_COUNT,
        "converted": [],
        "errors": [],
        "saved": False,
    }

    with open(SOURCE_T3D, "r", encoding="utf-8-sig") as stream:
        source_text = stream.read()

    target_names = set(
        re.findall(
            r"Begin Actor Class=/Script/InteriorPCGRuntime\.InteriorPCGStaticMeshActor Name=([^ ]+)",
            source_text,
        )
    )
    if len(target_names) != EXPECTED_COUNT:
        raise RuntimeError(
            f"Attachment target count mismatch: expected {EXPECTED_COUNT}, found {len(target_names)}"
        )

    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = actor_subsystem.get_all_level_actors()
    targets = [actor for actor in all_actors if actor.get_name() in target_names]
    missing = sorted(target_names - {actor.get_name() for actor in targets})
    if missing:
        raise RuntimeError(
            f"Loaded map exposes only {len(targets)}/{EXPECTED_COUNT} targets. "
            f"No changes made. Missing examples: {missing[:10]}"
        )

    staged = []
    try:
        for source_actor in targets:
            source_component = source_actor.get_component_by_class(unreal.StaticMeshComponent)
            if not source_component:
                raise RuntimeError(f"{source_actor.get_name()}: no StaticMeshComponent")

            static_mesh = source_component.get_editor_property("static_mesh")
            if not static_mesh:
                raise RuntimeError(f"{source_actor.get_name()}: no StaticMesh assigned")

            source_transform = source_component.get_world_transform()
            source_location = source_transform.translation
            source_rotation = source_transform.rotation.rotator()
            source_scale = source_transform.scale3d
            source_materials = get_component_material_paths(source_component)

            new_actor = actor_subsystem.spawn_actor_from_class(
                unreal.StaticMeshActor,
                source_location,
                source_rotation,
            )
            if not new_actor:
                raise RuntimeError(f"{source_actor.get_name()}: failed to spawn StaticMeshActor")

            new_actor.set_actor_scale3d(source_scale)
            new_actor.set_actor_label(source_actor.get_actor_label() + "_Static", mark_dirty=True)
            try:
                new_actor.set_folder_path("Interior_Static")
            except Exception:
                pass

            new_component = new_actor.get_component_by_class(unreal.StaticMeshComponent)
            new_component.set_static_mesh(static_mesh)
            for index in range(source_component.get_num_materials()):
                material = source_component.get_material(index)
                if material:
                    new_component.set_material(index, material)

            for property_name in (
                "cast_shadow",
                "cast_dynamic_shadow",
                "cast_static_shadow",
                "visible",
                "hidden_in_game",
                "receives_decals",
                "render_custom_depth",
                "custom_depth_stencil_value",
                "custom_depth_stencil_write_mask",
                "can_ever_affect_navigation",
                "collision_profile_name",
                "body_instance",
                "mobility",
            ):
                copy_editor_property_if_available(source_component, new_component, property_name)

            for property_name in ("tags", "layers", "is_editor_only_actor"):
                copy_editor_property_if_available(source_actor, new_actor, property_name)

            new_transform = new_component.get_world_transform()
            location_error = vector_delta(source_transform.translation, new_transform.translation)
            rotation_error = rotation_delta(
                source_transform.rotation.rotator(), new_transform.rotation.rotator()
            )
            scale_error = vector_delta(source_transform.scale3d, new_transform.scale3d)
            new_materials = get_component_material_paths(new_component)

            if location_error > LOCATION_TOLERANCE:
                raise RuntimeError(
                    f"{source_actor.get_name()}: location mismatch {location_error} cm"
                )
            if rotation_error > ROTATION_TOLERANCE:
                raise RuntimeError(
                    f"{source_actor.get_name()}: rotation mismatch {rotation_error} degrees"
                )
            if scale_error > SCALE_TOLERANCE:
                raise RuntimeError(
                    f"{source_actor.get_name()}: scale mismatch {scale_error}"
                )
            if new_component.get_editor_property("static_mesh") != static_mesh:
                raise RuntimeError(f"{source_actor.get_name()}: StaticMesh mismatch")
            if new_materials != source_materials:
                raise RuntimeError(f"{source_actor.get_name()}: material mismatch")

            staged.append((source_actor, new_actor))
            report["converted"].append(
                {
                    "source": source_actor.get_name(),
                    "replacement": new_actor.get_name(),
                    "mesh": static_mesh.get_path_name(),
                    "location_error_cm": location_error,
                    "rotation_error_degrees": rotation_error,
                    "scale_error": scale_error,
                    "materials": source_materials,
                }
            )

        if len(staged) != EXPECTED_COUNT:
            raise RuntimeError(f"Staged count mismatch: {len(staged)}/{EXPECTED_COUNT}")

        for source_actor, _ in staged:
            if not actor_subsystem.destroy_actor(source_actor):
                raise RuntimeError(f"Failed to destroy source actor {source_actor.get_name()}")

        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        report["saved"] = True
        unreal.log(f"Converted and verified {len(staged)} Interior PCG actors to StaticMeshActor")
    except Exception:
        for _, new_actor in staged:
            try:
                actor_subsystem.destroy_actor(new_actor)
            except Exception:
                pass
        raise
    finally:
        os.makedirs(os.path.dirname(REPORT_PATH), exist_ok=True)
        with open(REPORT_PATH, "w", encoding="utf-8") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)


try:
    main()
except Exception as error:
    unreal.log_error(str(error))
    unreal.log_error(traceback.format_exc())
    raise
