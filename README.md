# Gangstar Rio for PS Vita

A work-in-progress loader of **Gangstar Rio: City of Saints** for PlayStation Vita, using the Android ARMv7 version of the game.

Menus, the opening cutscene, gameplay and sound work. The game still has streaming stutters, and it has not been tested through every mission.

## Get started

1. [Build the VPK](docs/BUILDING.md), or use a VPK supplied with a release.
2. [Install the game and copy your game files](docs/INSTALLING.md).

You need your own copy of the supported Android game. The repository and VPK do not include the APK, OBBs or original game libraries.

## Controls

| Control | On foot | Driving |
| --- | --- | --- |
| Left stick | Move | Steer |
| Right stick | Camera | Camera |
| Cross (hold) | Sprint | Nitro (release; when available) |
| Square (hold) | Punch or fire | Fire |
| R | — | Accelerate |
| Circle / L | Take cover (release) | Circle: toggle radio; L: brake |
| Triangle | Get up when fallen (tap repeatedly); otherwise nearby shop/item interaction or enter vehicle | Exit vehicle (release) |
| D-pad left / right | Previous / next weapon | Previous / next weapon |
| D-pad up | Reload | Toggle radio |
| D-pad down | Use selected consumable | Vehicle side mission (when available) |
| Select | Map / menu | Map / menu |
| Start | Pause (original gamepad input) | Pause (original gamepad input) |
| Touchscreen | Menus and contextual prompts | Menus and contextual prompts |

Aircraft use the left stick for pitch/roll through the game's native aircraft stick controls; R/L provide the native accelerate/brake inputs. Physical tilt steering is disabled for cars and aircraft. Aircraft controls still need mission testing on hardware.

## Building the current version

With VitaSDK and the required libraries installed, open a terminal in this directory and run:

```sh
bash build.sh
```

The VPK will be at `work/build/gangstar_rio_wip.vpk`.

The default build uses the current preferred gameplay settings. You do not need to set performance options or copy anyone else's build cache. See the [build guide](docs/BUILDING.md) for setup and a clean build.

## Contributing

Source is in `source/`, required support libraries are in `lib/`, and host tests are in `tools/`. Read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting changes.

The original loader's MIT license is retained in [LICENSE](LICENSE). Other components keep their own licenses; see [third-party notices](docs/THIRD_PARTY_NOTICES.md). Gangstar Rio belongs to its respective owners. This project is not affiliated with Gameloft.
