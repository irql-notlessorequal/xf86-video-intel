# xf86-video-intel

Open-source X.org graphics driver for Intel graphics

[https://01.org/linuxgraphics/
](https://web.archive.org/web/20200430064829/https://01.org/linuxgraphics/)

## What is xf86-video-intel
The xf86-video-intel module is an open-source 2D graphics driver for
the X Window System as implemented by X.org.

It supports a variety of Intel graphics chipsets including:

```
	i810/i810e/i810-dc100,i815,
	i830M,845G,852GM,855GM,865G,
	915G/GM,945G/GM/GME,946GZ
	G/GM/GME/Q965,
	G/Q33,G/Q35,G41,G/Q43,G/GM/Q45
	PineView-M (Atom N400 series)
	PineView-D (Atom D400/D500 series)
	Intel(R) HD Graphics,
	Intel(R) Iris(TM) Graphics,
	Intel(R) Iris(TM) Pro Graphics.
```

## Where to get more information about the driver

The primary source of information about this and other open-source
drivers for Intel graphics is: [https://01.org/linuxgraphics/
](https://web.archive.org/web/20200430064829/https://01.org/linuxgraphics/)

Documentation specific to the xf86-video-intel driver including
possible configuration options for the xorg.conf file can be found in
the `intel(4)` manual page. After installing the driver this
documentation can be read with the following command:

```
	man intel
```

## Notable changes over the regular xf86-video-intel implementation.

1. Fix for screen freezing if using PRIME and a secondary graphics card.
2. Restoration and clean up of legacy options.
3. Internal Y-tiling preference by default (Ivy Bridge and newer)
4. Implementation of `CreateBuffer2`, `DestroyBuffer2` and `CopyRegion2` within SNA and UXA.
5. Improved performance with the BLT (blitter) engine on Gen 7.x (Ivy Bridge/Haswell) era hardware when mitigations are disabled.

## Known issues.

### Broken hardware acceleration (EGL only) when using Nvidia drivers.

Fixed in `master` branch as of November 7th 2024.

### Lack of support on newer hardware.

Generation 11 (Ice Lake) and newer graphics are not supported.

There are no plans to address this, a solution is to use the `modesetting` driver included within XOrg.

### Missing symbols (i.e. `vbeFree`)

Fixed in `master` branch as of July 2025.