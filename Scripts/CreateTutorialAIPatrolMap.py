import math
import random

import unreal


MAP_PATH = "/Game/Maps/Tutorial_AI_Patrol"
ASSET_ROOT = "/Game/ModularSciFiStation/Environment"


def load_asset(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        unreal.log_warning(f"Missing asset: {path}")
    return asset


def spawn_mesh(asset_path, location, rotation=(0.0, 0.0, 0.0), scale=(1.0, 1.0, 1.0), label=None):
    asset = load_asset(asset_path)
    if not asset:
        return None

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    asset_path_no_object = asset_path.split(".")[0]

    if isinstance(asset, unreal.StaticMesh):
        actor = actor_subsystem.spawn_actor_from_class(
            unreal.StaticMeshActor,
            unreal.Vector(*location),
            unreal.Rotator(*rotation),
        )
        mesh_component = actor.get_component_by_class(unreal.StaticMeshComponent)
        mesh_component.set_static_mesh(asset)
    else:
        blueprint_class = unreal.EditorAssetLibrary.load_blueprint_class(asset_path_no_object)
        if not blueprint_class:
            unreal.log_warning(f"Could not load blueprint class: {asset_path_no_object}")
            return None
        actor = actor_subsystem.spawn_actor_from_class(
            blueprint_class,
            unreal.Vector(*location),
            unreal.Rotator(*rotation),
        )

    actor.set_actor_scale3d(unreal.Vector(*scale))
    if label:
        actor.set_actor_label(label)
    return actor


def spawn_class(actor_class, location, rotation=(0.0, 0.0, 0.0), scale=(1.0, 1.0, 1.0), label=None):
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actor_subsystem.spawn_actor_from_class(
        actor_class,
        unreal.Vector(*location),
        unreal.Rotator(*rotation),
    )
    actor.set_actor_scale3d(unreal.Vector(*scale))
    if label:
        actor.set_actor_label(label)
    return actor


def set_mobility(actor, mobility):
    components = actor.get_components_by_class(unreal.SceneComponent)
    for component in components:
        if hasattr(component, "set_mobility"):
            component.set_mobility(mobility)


def set_light(actor, intensity=None, radius=None, color=None):
    light = actor.get_component_by_class(unreal.LightComponent)
    if not light:
        return
    if intensity is not None:
        light.set_editor_property("intensity", intensity)
    if radius is not None and hasattr(light, "attenuation_radius"):
        light.set_editor_property("attenuation_radius", radius)
    if color is not None:
        light.set_editor_property("light_color", color)


def make_floor():
    floor_a = f"{ASSET_ROOT}/Floor/SM_floor_400x400_01.SM_floor_400x400_01"
    floor_b = f"{ASSET_ROOT}/Floor/SM_floor_400x400_02.SM_floor_400x400_02"
    for x in range(-3, 4):
        for y in range(-4, 5):
            asset = floor_a if (x + y) % 2 == 0 else floor_b
            spawn_mesh(asset, (x * 400.0, y * 400.0, 0.0), label=f"Floor_{x}_{y}")


def make_perimeter():
    wall_a = f"{ASSET_ROOT}/Walls/A/SM_wall_400_a_01.SM_wall_400_a_01"
    wall_b = f"{ASSET_ROOT}/Walls/B/SM_wall_400_b_02.SM_wall_400_b_02"
    wall_door = f"{ASSET_ROOT}/Walls/A/SM_wall_door_400_a_01.SM_wall_door_400_a_01"
    door = f"{ASSET_ROOT}/Door/BP_door_01.BP_door_01"
    pillar = f"{ASSET_ROOT}/Walls/SM_wall_pillar_100_01.SM_wall_pillar_100_01"

    for x in range(-3, 4):
        north_asset = wall_door if x == 0 else wall_a
        spawn_mesh(north_asset, (x * 400.0, 2000.0, 0.0), (0.0, 0.0, 0.0), label=f"NorthWall_{x}")
        spawn_mesh(wall_b, (x * 400.0, -2000.0, 0.0), (0.0, 180.0, 0.0), label=f"SouthWall_{x}")

    spawn_mesh(door, (0.0, 2020.0, 0.0), (0.0, 0.0, 0.0), label="Tutorial_Entrance_Door")

    for y in range(-4, 5):
        spawn_mesh(wall_a, (-1600.0, y * 400.0, 0.0), (0.0, 90.0, 0.0), label=f"WestWall_{y}")
        spawn_mesh(wall_b, (1600.0, y * 400.0, 0.0), (0.0, -90.0, 0.0), label=f"EastWall_{y}")

    for x in (-1600.0, 1600.0):
        for y in (-2000.0, 2000.0):
            spawn_mesh(pillar, (x, y, 0.0), (0.0, 0.0, 0.0), label="Corner_Pillar")


def make_cover_and_guides():
    pillar_200 = f"{ASSET_ROOT}/Scaffold/SM_hydraulic_pillar_200_01.SM_hydraulic_pillar_200_01"
    pillar_400 = f"{ASSET_ROOT}/Scaffold/SM_hydraulic_pillar_400_01.SM_hydraulic_pillar_400_01"
    railing = f"{ASSET_ROOT}/Railing/SM_railing_200_01.SM_railing_200_01"
    console = f"{ASSET_ROOT}/Props/WallAttachments/SM_control_panel_01.SM_control_panel_01"
    cable = f"{ASSET_ROOT}/Props/CableCover/SM_cable_cover_400_01.SM_cable_cover_400_01"
    vent = f"{ASSET_ROOT}/Props/Ventilation/SM_vent_01.SM_vent_01"

    cover_layout = [
        (pillar_200, (-850.0, -1050.0, 0.0), 20.0, "Cover_Low_Left_A"),
        (pillar_400, (750.0, -900.0, 0.0), -15.0, "Cover_Tall_Right_A"),
        (railing, (-500.0, -250.0, 0.0), 90.0, "Cover_Railing_Mid_A"),
        (railing, (550.0, 250.0, 0.0), -90.0, "Cover_Railing_Mid_B"),
        (pillar_200, (-900.0, 700.0, 0.0), -30.0, "Cover_Low_Left_B"),
        (pillar_400, (950.0, 850.0, 0.0), 35.0, "Cover_Tall_Right_B"),
        (railing, (0.0, -1250.0, 0.0), 0.0, "Cover_Railing_Start"),
        (railing, (0.0, 1150.0, 0.0), 180.0, "Cover_Railing_End"),
    ]
    for asset, loc, yaw, label in cover_layout:
        spawn_mesh(asset, loc, (0.0, yaw, 0.0), label=label)

    for x in (-1200.0, 1200.0):
        for y in (-1200.0, 0.0, 1200.0):
            spawn_mesh(console, (x, y, 115.0), (0.0, 90.0 if x < 0 else -90.0, 0.0), label="Wall_Control_Panel")

    for y in (-1200.0, -400.0, 400.0, 1200.0):
        spawn_mesh(cable, (0.0, y, 8.0), (0.0, 90.0, 0.0), label="Floor_Cable_Cover")

    for x in (-800.0, 800.0):
        spawn_mesh(vent, (x, 1980.0, 240.0), (0.0, 180.0, 0.0), label="North_Wall_Vent")


def make_lighting():
    lamp = f"{ASSET_ROOT}/Lamps/BP_Ceiling_Lamp_200.BP_Ceiling_Lamp_200"
    wall_lamp = f"{ASSET_ROOT}/Lamps/BP_Wall_Lamp_120.BP_Wall_Lamp_120"
    red_lamp = f"{ASSET_ROOT}/Lamps/BP_Wall_Lamp_80_red.BP_Wall_Lamp_80_red"

    for x in (-800.0, 0.0, 800.0):
        for y in (-1200.0, 0.0, 1200.0):
            spawn_mesh(lamp, (x, y, 380.0), (0.0, 0.0, 0.0), label="Ceiling_Lamp")

    for y in (-1200.0, 0.0, 1200.0):
        spawn_mesh(wall_lamp, (-1580.0, y, 220.0), (0.0, 90.0, 0.0), label="West_Wall_Lamp")
        spawn_mesh(wall_lamp, (1580.0, y, 220.0), (0.0, -90.0, 0.0), label="East_Wall_Lamp")

    spawn_mesh(red_lamp, (0.0, 1900.0, 240.0), (0.0, 180.0, 0.0), label="Exit_Red_Lamp")

    sun = spawn_class(unreal.DirectionalLight, (0.0, 0.0, 1200.0), (-55.0, -35.0, 0.0), label="Tutorial_Key_Light")
    set_light(sun, 1.5)
    sky = spawn_class(unreal.SkyLight, (0.0, 0.0, 400.0), label="Tutorial_Sky_Light")
    set_light(sky, 0.6)
    fog = spawn_class(unreal.ExponentialHeightFog, (0.0, 0.0, 0.0), label="Tutorial_Fog")
    fog_component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    if fog_component:
        fog_component.set_editor_property("fog_density", 0.01)


def make_navigation_and_starts():
    nav = spawn_class(unreal.NavMeshBoundsVolume, (0.0, 0.0, 120.0), scale=(18.0, 22.0, 4.0), label="Tutorial_NavMeshBounds")

    patrol_points = [
        (-1050.0, -1350.0, 90.0),
        (950.0, -1250.0, 90.0),
        (1050.0, 1000.0, 90.0),
        (-950.0, 1200.0, 90.0),
        (-250.0, 0.0, 90.0),
        (650.0, 100.0, 90.0),
    ]
    for index, point in enumerate(patrol_points, start=1):
        spawn_class(unreal.TargetPoint, point, label=f"PatrolRoute_A_{index:02d}")

    player_starts = [
        (-650.0, -1650.0, 110.0, 35.0),
        (0.0, -1650.0, 110.0, 0.0),
        (650.0, -1650.0, 110.0, -35.0),
    ]
    for index, (x, y, z, yaw) in enumerate(player_starts, start=1):
        spawn_class(unreal.PlayerStart, (x, y, z), (0.0, yaw, 0.0), label=f"Random_PlayerStart_{index:02d}")

    for index, (x, y, z) in enumerate([(-900.0, 1000.0, 110.0), (900.0, -900.0, 110.0)], start=1):
        enemy = spawn_class(unreal.load_class(None, "/Script/UnrealTPS.MeleeEnemyCharacter"), (x, y, z), (0.0, random.choice([90.0, -90.0, 180.0]), 0.0), label=f"Placed_Patrol_Enemy_{index:02d}")
        if enemy:
            enemy.set_editor_property("patrol_radius", 900.0)

    return nav


def configure_world():
    world = unreal.EditorLevelLibrary.get_editor_world()
    settings = world.get_world_settings()
    game_mode = unreal.load_class(None, "/Script/UnrealTPS.TPSGameMode")
    if game_mode:
        settings.set_editor_property("default_game_mode", game_mode)


def build_map():
    unreal.EditorAssetLibrary.make_directory("/Game/Maps")
    if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
        raise RuntimeError(f"Could not create level {MAP_PATH}")

    make_floor()
    make_perimeter()
    make_cover_and_guides()
    make_lighting()
    make_navigation_and_starts()
    configure_world()

    unreal.EditorLoadingAndSavingUtils.save_current_level()
    unreal.log(f"Created tutorial AI patrol map: {MAP_PATH}")


build_map()
