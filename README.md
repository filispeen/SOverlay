# SOverlay

Native OBS Studio plugin plus a transparent Electron overlay window that mirrors OBS scene sources in real time.

SOverlay lets a source live "on screen" for the streamer without OBS rendering it into the final stream output, or vice versa: rendered by OBS but hidden from the on-screen overlay. OBS stays the single source of truth for position, visibility, and media state; the Electron overlay just reflects it.

## How it works

- **OBS plugin** (`plugin/`, C++): a source filter you attach to any Source. Adds a `Show onscreen` property. When enabled, the filter skips OBS's render of that source and instead broadcasts its state (position, scale, z-index, media state, etc.) over a local WebSocket server the plugin hosts.
- **Electron overlay** (`SOverlay/`): connects to that WebSocket, mirrors each flagged source into its own always-on-top, click-through, transparent window, positioned and scaled to match the OBS canvas.

```
OBS Source --[Show onscreen]--> plugin filter --skip render--> ws-hub (127.0.0.1:7853)
                                                 |                      |
                                                 v                      v
                                          doesn't work :D      Electron overlay windows
```

## Behavior

| Enabled | Show onscreen | Rendered by OBS | Shown by overlay |
|---|---|---|---|
| true | true | true | yes |
| true | false | yes | no |
| false | - | no | no |

## Supported sources

- Browser sources (offscreen-rendered, live CSS injection support)
- Image and local media/video sources (including loop and scrub/seek state)
- Any other source type for basic position/visibility mirroring

## Requirements

- Windows
- OBS Studio (plugin built against OBS SDK 32.2.2)

## Install

1. Download the installer from Releases, or build from source (see below).
2. The installer locates your OBS Studio install (registry lookup, common Steam/standard paths, or manual Browse) and copies the plugin DLL into `obs-plugins/64bit`.
3. Launch OBS. The plugin auto-starts the Electron overlay app and keeps it running alongside OBS.

## Usage

1. Add `SOverlay` as a filter on any Source in OBS.
2. Toggle **Show onscreen** in the filter properties.
3. The source disappears from your OBS program output and appears in the transparent overlay window on your desktop, positioned to match its OBS transform.

[example.mp4](md/example.mp4)


## Building from source

**Plugin**

```
cmake --preset windows-ci-x64
cmake --build plugin/build_x64 --config Release
```

Output: `plugin/build_x64/rundir/Release/soverlay-obs-plugin.dll`

**Electron app**

```
npm install
npm start        # dev
npm run make      # electron-forge build
```

## License

MIT
