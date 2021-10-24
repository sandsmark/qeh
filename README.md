qeh
---

Like feh, but better, faster, more image formats, simpler, more lightweight (73kb),
animation support, and better UI (there is none).

![screenshot](/screenshot.png)

Supports everything from Photoshop and Gimp images via animated gifs to SunOS
raster images and DirectX textures, assuming you have the right plugins
installed (kimageformats).

Uses optimized SSE4.1 code and threadpools for high quality scaling and is
generally good (thanks to carewolf who implemented it in Qt).

Keyboard shortcuts
------------------

 - Equals/Plus/Up: Zooms in
 - Minus/Down: Zooms out
 - Right: Move to the right side of the screen
 - Left: Move to the left side of the screen
 - Escape/Q: Quit
 - F: Maximize
 - I: Show/hide image info
 - 1-0: Zooms from 10% to 100% respectively.
 - Space: Pause/continue animation
 - W: Increase animation speed with 10%
 - S: Decrease animation speed with 10%
 - D: Pause and step forward in animation
 - A: Pause and step backward in animation
 - E: Equalize image
 - N: Normalize image
 - Backspace: Reset playback speed, reset zoom


Plugins
-------

Non-exhaustive list, not everything might be packed for your distro:

 - [Qt Image Formats](https://doc.qt.io/qt-5/qtimageformats-index.html): WebP, JPEG 2000, Targa, TIFF, Wireless Bitmap, MNG, DirectX textures, Apple icons
 - [Qt SVG](https://doc.qt.io/qt-5/qtsvg-index.html): SVG (obviously)
 - [Qt WebEngine](https://doc.qt.io/QT-5/qtwebengine-index.html): PDF
 - [KImageFormats](https://github.com/KDE/kimageformats): Windows cursors, AVIF, EPS, OpenEXR, Radiance HDR, HEIF, Krita, OpenRaster, Gimp, pic, Photoshop, SunOS raster, SGI RGB, Targa
 - [QtRaw](https://github.com/sandsmark/qtraw): RAW images, various formats (via libraw)
 - [QVTF](https://github.com/HurricanePootis/qvtf): Valve Texture Files
 - [qt-jpegxl-image-plugin](https://github.com/novomesk/qt-jpegxl-image-plugin/): JPEG-XL images
 - [QtApng](https://github.com/Skycoder42/QtApng): Animated PNG
 - [QtPBFImagePlugin](https://github.com/tumic0/QtPBFImagePlugin): Mapbox vector tiles
 - [extra-imageformats-qt](https://github.com/sandsmark/extra-imageformats-qt): BPG, PGF and FUIF


In addition there's the built-in ones (at the moment, might be more whenever you're reading this): BMP, GIF, JPG, JPEG, PNG, PBM, PGM, PPM, XBM, XPM,
