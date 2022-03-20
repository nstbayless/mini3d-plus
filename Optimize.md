Here are some things which should be investigated to optimize the device.

- TEXTURES_ALWAYS_POWER_OF_2 -- should save some % calls
- PRECOMPUTE_PROJECTION
- various techniques to do less texture projection on surfaces that don't need it
- interlace -- does it help??
- prefetch the backbuf (if being used) / regular buff
- ues the backbuf?