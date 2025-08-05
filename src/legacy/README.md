# Welcome to the attic!

> The code contained herein is no longer under active development and is not the
> target for new features or other improvements. Instead, we have placed it here
> to mature gracefully and still provide hardware compatibility for those
> antiquated devices that turn up when you least expect them.
\- Intel

## DEPRECATED

This driver is not supported and not built by default as it relies on User Mode Setting (UMS)
which has been removed in Linux 6.7.

If this driver was compiled with `-fno-plt` you must add the following section to your X.Org config.

```
Section "Module"
	Load "vgahw"
	Load "shadowfb"
	Load "int10"
EndSection
```