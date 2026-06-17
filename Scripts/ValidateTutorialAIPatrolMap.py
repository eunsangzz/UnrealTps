import unreal


MAP_PATH = "/Game/Maps/Tutorial_AI_Patrol"


def main():
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError(f"Could not load {MAP_PATH}")

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors()
    labels = [actor.get_actor_label() for actor in actors]

    checks = {
        "PlayerStarts": len([label for label in labels if label.startswith("Random_PlayerStart_")]),
        "PatrolPoints": len([label for label in labels if label.startswith("PatrolRoute_A_")]),
        "CoverActors": len([label for label in labels if label.startswith("Cover_")]),
        "NavMeshBounds": len([label for label in labels if label == "Tutorial_NavMeshBounds"]),
        "PlacedEnemies": len([label for label in labels if label.startswith("Placed_Patrol_Enemy_")]),
    }

    unreal.log(f"Tutorial map actor count: {len(actors)}")
    for name, count in checks.items():
        unreal.log(f"{name}: {count}")

    required = {
        "PlayerStarts": 3,
        "PatrolPoints": 6,
        "CoverActors": 8,
        "NavMeshBounds": 1,
        "PlacedEnemies": 2,
    }
    for name, minimum in required.items():
        if checks[name] < minimum:
            raise RuntimeError(f"{name} expected at least {minimum}, got {checks[name]}")


main()
