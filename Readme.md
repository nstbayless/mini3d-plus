Enhanced 3D engine for the Panic Playdate. Based on the mini3d library provided in the examples.

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