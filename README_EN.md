# SnapCapture (CaptureTool)

[中文版](README.md)

SnapCapture is a lightweight, native Windows screenshot and annotation tool built using C++11, Win32 API, and GDI/GDI+.

## Features

*   **Lightweight & No Dependencies**: Built purely with Win32 SDK and GDI+. Free of third-party UI framework dependencies. Can be statically compiled into a single standalone executable (approx. 1.2 MB with symbols stripped).
*   **Low Resource Footprint**: Uses double-buffered rendering and partial updates. Idle memory footprint is only about 2.5 MB.
*   **DPI-Aware**: Seamlessly supports system-level High DPI scaling and multi-monitor setups, ensuring crisp screenshot capturing and rendering.

## What's New in v1.3.0

*   **New: Mosaic tool** — brush (6-150px continuous slider, live coverage-circle cursor) and rectangular modes; quality (4-40px blocks) on a continuous slider; pixelate/blur styles; persisted settings; undo/redo supported.
*   **New: Scrolling long capture** — frame a region, scroll the page yourself; the app passively observes and stitches a full-length image. A floating side panel previews the result live (narrowing smoothly as it grows); when done, the full-resolution capture goes back into the frame for the regular copy/save flow. No input injection, no focus stealing, no cursor moves; scrolling back never duplicates content.
*   **Fixed: text annotation offset** — committed text now matches the on-screen preview exactly; fixed emoji (surrogate pair) editing/deletion.
*   **Improved: rendering performance** — cursor, strokes and hover updates redraw only the changed region for high frame rates.
*   **UI** — rounded toolbar/panels/color chips, redesigned vector icons, slider controls, toolbar slide-in animation and icon hover feedback.

## Download

Prefer not to build it yourself? Grab the latest `CaptureTool.exe` from the
[Releases page](https://github.com/combodevy/SnapCapture/releases). No installer, and nothing is written outside its own folder.

## Core Functions

1.  **Screenshot Mode & Region Selection**
    *   **Window Detection**: Utilizes DWM (Desktop Window Manager) API to retrieve exact physical window boundaries (excluding drop shadows), with hover-highlighting and single-click capturing.
    *   **Freehand Selection**: Drag with the left mouse button to select any custom region, with eight-way resize handles and global panning support.
2.  **Vector Annotation Tools**
    *   **Basic Shapes**: Draw rectangular frames (with custom, persistent round corner radius), ellipses, and vector arrows.
    *   **Mosaic Redaction**: Brush (continuous 6-150px width slider with a live coverage-circle cursor) and rectangular mosaic modes, quality (4-40px blocks, continuous slider, persisted), with pixelate/blur styles; undo / redo applies as well.
    *   **Properties Adjustment**: Adjust brush thickness (2px, 4px, 8px) and annotation colors via the floating toolbar (with custom color selection).
    *   **Undo / Redo**: Step back through applied annotations and redo them again, from either the toolbar button or the keyboard.
3.  **Text Annotation Engine**
    *   **Inline Editing**: Double-click any text element to edit it directly in-place. Supports arrow keys, Home/End cursor navigation, Backspace/Delete, and custom text insertions.
    *   **Styling & Transforms**: Easily choose font family, font size, bold, and italic options using the Windows ChooseFont dialog. Resize text proportionally via corner handles or rotate text seamlessly from 0° to 360°.
    *   **IME Input**: Composition strings are rendered inline at the caret with an underline, and the candidate window follows the text caret instead of sticking to a screen corner.
4.  **System Integration**
    *   **Global Hotkey**: Register a keyboard combination or mouse side buttons (Mouse4/Mouse5) as hotkeys, with optional key suppression to prevent conflicts.
    *   **Clipboard & Saving**: Copies to clipboard in dual formats (`CF_DIB` for Paint and Office, `PNG` for Discord, Slack and other modern apps), or saves to disk - either via a dialog or automatically to a configured directory.
    *   **Startup on Boot**: Run at startup via the Windows Registry `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` (no admin privileges required).

## Configuration (config.json)

The configuration file is automatically generated in the same directory as the executable:

```json
{
  "hotkey": {
    "type": "mouse",
    "keys": [],
    "mouse_button": "x1",
    "suppress": true
  },
  "auto_start": true,
  "save_to_clipboard": true,
  "save_directory": "",
  "notification": true,
  "round_radius": 10,
  "capture_mode": "region"
}
```

## Compilation

### Prerequisites
*   **OS**: Windows 7 or higher
*   **Compiler**: MinGW-w64 (GCC 11.x or higher supporting C++11)
*   **Shell**: PowerShell

### Steps
1. Open PowerShell and navigate to the project root directory.
2. Run the build script:
   ```powershell
   .\build.ps1
   ```

### Build script parameters

| Parameter | Description | Default |
| --- | --- | --- |
| `-OutputName` | Name of the produced executable | `CaptureTool.exe` |
| `-Architecture` | Target architecture: `x64` or `x86` | `x64` |
| `-Strip` | Strip symbols after linking to reduce size | Off |
| `-Run` | Launch the executable after a successful build | Off |

Example:

```powershell
.\build.ps1 -Strip -Run
```

### Compilation flags

The script runs the equivalent of:

```powershell
g++ -std=c++11 -O3 -mwindows -static main.cpp -lgdi32 -lgdiplus -lshlwapi -luser32 -lshell32 -lole32 -lcomdlg32 -ldwmapi -limm32 -o CaptureTool.exe
```

## CI & Release

GitHub Actions handles continuous integration and releases:

*   **Build Check** (`.github/workflows/build.yml`): every push or pull request triggers a real MinGW-w64 build on `windows-latest`, and uploads `CaptureTool.exe` as a build artifact.
*   **Release** (`.github/workflows/release.yml`): pushing a `v*` tag builds a stripped binary and publishes a GitHub Release with `CaptureTool.exe` attached.

To publish a new version:

```powershell
git tag v1.0.0
git push origin v1.0.0
```

The Release workflow can also be triggered manually from GitHub with an explicit tag.

> The compiled executable is no longer committed to the repository. Get it from the Releases page or from the Actions build artifacts.

## Usage

1.  **Run**: Launch `CaptureTool.exe` to run silently in the system tray.
2.  **Trigger**: Press the configured hotkey (default: Mouse Side Button X1) to start capturing.
3.  **Capture**:
    *   Hover over any window and left-click to capture it instantly.
    *   Or, click and drag to select a custom region.
4.  **Annotate**:
    *   Select tools (Rectangle, Circle, Arrow, Pencil, Text) from the floating toolbar.
    *   Customize color, thickness, roundness, or font styling on the fly.
5.  **Save/Output**:
    *   **Confirm (Checkmark icon)**: Copies to clipboard according to config, and auto-saves a PNG when a save directory is configured.
    *   **Save (Floppy Disk icon)**: Prompts for a PNG destination, and copies to clipboard according to config.
    *   **Cancel (Cross icon) / Esc**: Exits without saving.

6.  **Keyboard shortcuts**:
    *   `Enter` or `Ctrl + C`: confirm (copies to clipboard, and auto-saves a PNG when a save directory is set) and exit
    *   `Ctrl + S`: save as PNG
    *   `Ctrl + Z`: undo the last annotation
    *   `Ctrl + Y` or `Ctrl + Shift + Z`: redo
    *   `Esc`: exit without saving

7.  **Settings**: right-click the tray icon and open Settings to change the global hotkey, clipboard copying, the auto-save directory, tray notifications, the default capture mode, and launch on boot. If the hotkey ever stops responding, use the "重新装载快捷键钩子" (reload hooks) entry in the same menu.

## Known Limitations

*   Long capture relies on the page responding to scrolling; pages full of animations/videos may not stitch well. The finished image cannot be re-annotated inside the frame.
*   Long capture only stitches **downward**: scrolling back up never breaks or duplicates what was captured, but it also adds nothing. To capture content above your starting region, re-frame and start over.
*   No delayed capture.
*   Undo rewinds in the order annotations were added; deleting a selected element is not separately undoable.
*   Single instance: launching a second copy only shows a warning, it does not trigger a new capture.

## License

Released under the [MIT License](LICENSE).
