# OBS Beauty Camera

A fully local, GPU-accelerated beauty camera source and filter for OBS Studio on Windows x64. No account, no cloud upload, no subscription, and no online authorization.

The plugin adds a **Beauty Camera** source with a built-in camera capture and a **Beauty Camera Filter** that can be applied to any existing video source. All processing runs on the local GPU through a single Direct3D 11 shader pass, with lightweight CPU face tracking.

> Independent project. Not affiliated with or endorsed by OBS Studio or OpenAI.

## Features

- Automatic face tracking for up to 3 faces, with manual ellipse fallback
- Skin smoothing with texture protection, blemish/spot repair, whitening, rosy tone, clarity
- Eye brightening, teeth whitening, dark-circle reduction
- Face contouring, side shadow, face slimming, chin lift, eye enlargement
- Three makeup styles (natural, peach, rose) with lip, blush and eyeshadow controls
- Five color looks (clear, cream, film, cool white, warm skin)
- Body slimming and background blur approximations
- Glow, film grain, vignette, and split-screen before/after comparison
- Nine one-click presets, with **Refined** as the default
- Built-in illustrated Chinese manual and contact/support chapter
- Drag-in installation: merge `obs-plugins` and `data` into the OBS install directory

## Installation

1. Close OBS completely.
2. Download the release ZIP from the repository's **Releases** page.
3. Extract it and merge the `obs-plugins` and `data` folders into the OBS install directory (next to `obs64.exe`). Choose **Merge/Replace** when prompted.
4. Start OBS. Add **Beauty Camera** from **Sources**, or right-click any video source and add **Beauty Camera Filter** from **Filters**.

The plugin does not need an installer or registry changes. OBS scans `obs-plugins/64bit` and `data/obs-plugins` automatically at startup.

## Default look

New sources and filters start with the **Refined** preset: light smoothing, light whitening, preserved skin texture, moderate clarity and a neutral clear tone. The goal is a clean, realistic result without a plastic look. Nine presets are included; switch to **Custom** to adjust every slider.

## Build from source

Requirements:

- Windows 10/11 x64
- MinGW-w64 GCC (x86_64)
- OBS Studio 29.x headers and an `obs.a` import library generated from `obs.dll`

Build with the included Makefile:

```bash
make OBS_INCLUDE=/path/to/obs-studio/libobs OBS_LIBDIR=/path/to/obs-studio/build/libobs
```

Or directly:

```bash
gcc -shared -O2 -s -static-libgcc \
    -o obs-beauty-camera.dll src/plugin.c \
    -I<obs-studio>/libobs -L<lib-dir> \
    -lobs -lshell32 -ladvapi32
```

Place the DLL in `obs-plugins/64bit/`. Runtime data (shader, manual, locales) belongs in `data/obs-plugins/obs-beauty-camera/`. The source tree already contains the data files.

## Project layout

```text
src/plugin.c                                   OBS source/filter plugin (C)
data/obs-plugins/obs-beauty-camera/
  beauty.effect                                Direct3D 11 HLSL shader
  manual.html                                  Built-in illustrated manual
  contact.png                                  Contact/support card used by the manual
  locale/en-US.ini, locale/zh-CN.ini           UI translations
Makefile                                       MinGW build
README.zh-CN.md                                Chinese documentation
```

## How it works

- The source creates a private `dshow_input` child source and attaches a private beauty filter.
- The filter renders one HLSL pass with skin-color detection, a multi-face weight mask, bilateral smoothing, multi-radius spot repair, highlight-protected whitening, mid-tone rosy color, unsharp clarity, localized UV warping, eye/teeth/dark-circle regions, makeup tinting, color looks, background blur, glow/grain/vignette, and split comparison.
- Face tracking reads back a 160-pixel-wide GPU frame every 250 ms, runs CPU skin-color connected-component analysis, and smooths the detected centers/radii. If no face is found, the plugin falls back to the manual ellipse.
- All processing is local. The plugin never uploads video, requires no account, and performs no online authorization.

## Compatibility

- Windows 10/11 x64
- Built and tested with OBS Studio 29.1.3 x64
- Uses OBS's public plugin APIs; expected to work on OBS 28+ x64, but 28/30/31 have not been individually tested
- Requires `obs.dll`, OBS's bundled `win-dshow`, and the system UCRT
- 32-bit OBS is not supported

## Known limitations

- Face tracking uses skin-color connected components, not a deep-learning landmark model. Skin-colored backgrounds, extreme profile views, masks, or heavy occlusion can affect positioning; disable automatic tracking and use the manual ellipse in those cases.
- Body slimming and background blur are 2D screen-space approximations, not depth- or segmentation-based effects.
- Beauty processing is 2D screen-space; extreme angles or large occlusions may produce imperfect skin masks.
- The split-comparison line is about 1.5 pixels wide and appears thin at high resolutions.

## Privacy and licensing

The plugin is offline by design. It does not collect telemetry, upload frames, or contact a license server. The in-app manual contains optional contact and donation information; donations are voluntary and do not unlock any feature.

Licensed under the **GNU General Public License v2.0**. See [LICENSE](LICENSE).

## Changelog

See [CHANGELOG.md](CHANGELOG.md). Current release: **v2.1.0**.
