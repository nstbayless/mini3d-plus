# Mini3D+

Enhanced 3D engine for the Panic Playdate. Based on the mini3d library provided in the examples.

If you are currently using or intend to use mini3d on the playdate, you can directly swap it out for mini3d-plus.

![Kart Example](./etc/kart.gif)

With textures:

![Textured Kart Example](./etc/kart-textures.gif)

## New features

- Mesh clpping at camera (allows rendering faces which are partly behind the camera)
- Textures
  - To use greyscale textures, you must save the image with some extension other than `.png`, as the build script converts `.png` images into `.pdi`.
- Collision detection

## Known bugs (and workarounds)

- Sometimes textures seem to 'jump' or 'flex.' There are two reasons for this:
  1. projective texture mapping is disabled for faces that are close to flush with the surface or are far from the camera. Try increasing `TEXTURE_PROJECTIVE_RATIO_THRESHOLD` or disabling the check altogether.
  2. Actually, the second cause is not fully understood, but it appears to happen only when clipping quads. Try using triangles instead of quads. 

## Build Instructions

You must have installed the [Playdate SDK](https://play.date/dev/). Be sure to set the `PLAYDATE_SDK_PATH` environment variable, as described in the SDK installation instructions.

This will build and launch the kart example:

```
make pdc
PlaydateSimulator ./3DLibrary.pdx
```

## Credits

If you use mini3d+, please credit the following.

- Dave Hayden (or Panic Software)
- [spng](https://libspng.org/)
- [miniz](https://github.com/richgel999/miniz)
- NaOH
- gingerbeardman (testing)