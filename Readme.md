# Mini3D+

Enhanced 3D engine for the Panic Playdate. Based on the mini3d library provided in the examples.

If you are currently using or intend to use mini3d on the playdate, you can directly swap it out for mini3d-plus.

![Kart Example](./etc/kart.gif)

## New features

- Mesh clpping at camera (allows rendering faces which are partly behind the camera)
- Textures
- Collision detection

## Build Instructions

You must have installed the [Playdate SDK](https://play.date/dev/). Be sure to set the `PLAYDATE_SDK_PATH` environment variable, as described in the SDK installation instructions.

This will build and launch the kart example:

```
make pdc
PlaydateSimulator ./3DLibrary.pdx
```