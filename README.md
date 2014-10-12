# Walkabout

The Walkabout Navigation Toolkit for [Torque 3D](https://github.com/GarageGames/Torque3D).
Formerly a commercial addon, now open-source!
Leverage the full power of Recast/Detour for automatic navmesh generation and pathfinding.

## Installation

Once you've downloaded this repository, copy/paste the `Engine/` and `Tools/` folders into your Torque 3D installation (and deal with the conflicts that inevitably arise).
Also copy `game/tools/` to `My Projects/Your Project/game/tools/`.
Then add this line to `My Projects/Your Project/buildFiles/config/projectCode.conf`:

    includeModule('walkabout');

Make sure you're also not including the built-in `'navigation'` module, or there'll be trouble.

## Documentation

Open up [Readme.html](./Readme.html) for links to the documentation.
