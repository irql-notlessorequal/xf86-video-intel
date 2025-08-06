#ifndef GLAMOR_HACK_H
#define GLAMOR_HACK_H

#ifndef O_CLOEXEC
#define O_CLOEXEC	02000000	/* set close_on_exec */
#endif

#define GlyphPicture(glyph) ((PicturePtr *) ((glyph) + 1))

#endif // GLAMOR_HACK_H