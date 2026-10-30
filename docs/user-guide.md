<p align="center">
  <a href="../README.md"><img src="images/chaos-hdr-title.svg" alt="Chaos HDR — home" width="435" height="90"></a>
</p>

<p align="center">
  <a href="../README.md">Home</a> ·
  <a href="installation.md">Install</a> ·
  <strong>User guide</strong> ·
  <a href="../README.md#documentation">All docs</a> ·
  <a href="https://chaosfox26.github.io/chaos-hdr/">Website</a>
</p>

<p align="center"><img src="images/neon-divider.svg" alt="" width="900" height="18"></p>

# Using Chaos HDR

## Updates and desktop repair

Open Alt+Z and choose **Updates & repair**. If the effect cannot load, Alt+Z and
the Chaos HDR application launcher open this recovery window directly.

The window shows your installed version, release channel, last check and any
connection/setup error. Automatic checks contact this project's GitHub releases
once every 24 hours at most; **Check for updates** requests an extra manual
check. A newer applicable release produces **Chaos HDR has an update** and an
**Install** button. Experimental installations include prereleases; stable
installations exclude them. No update installs without your confirmation.

After a KWin update, **Rebuild for this desktop** rebuilds the existing source
even if no new Chaos HDR release exists. It preserves saved profiles and allowed
applications and restarts only Chaos HDR. If the running desktop differs from
the installed desktop version, sign out and back in first. Future KWin changes
may still need a new Chaos HDR release.

Release installation preserves settings. A source checkout must be clean and
must fast-forward to the release without replacing local commits; otherwise the
window explains why installation stopped. Rebuild remains available for local
development work. Setup progress/errors are local under `output/updates/`.

## Choose an application

1. Start the application using its normal launcher.
2. Press Alt+Z or open Chaos HDR from the app menu.
3. Select a running application window.
4. Click Allow application, then enable HDR, Sharpening, or both.
5. Adjust the image, then close the panel with its top-right arrow, Escape or Alt+Z.

Approval is explicit. Merely opening controls or highlighting an unapproved window leaves it unchanged. Approved window identities are remembered locally; reopening an app can select it automatically. Activating another approved window can switch the selected target. Only one window is processed at a time. Remove application revokes its remembered approval.

## Picture controls

| Control | Current range or behavior |
|---|---|
| HDR | Enables HDR conversion independently of sharpening |
| Sharpening | Enables sharpening independently of HDR |
| Engine | Auto, RTX HDR only or Shader only; see [HDR engines](#hdr-engines) |
| Peak brightness | Automatic from KDE’s configured display; RTX HDR supports 400–2,000 nits, the shader engine 100–10,000 nits |
| Middle greys | 10–100 nits |
| Contrast | −100% to +100% |
| Saturation | −100% to +100% |
| Sharpen intensity | 0–100%; zero disables sharpening |
| Ignore Film Grain | 0–100%; reduces amplification of fine grain during sharpening |
| Edge-aware sharpening | Sharpens real edges only, leaving noise and smooth gradients alone and limiting halos |
| Debanding | On by default. RTX HDR uses NVIDIA's debanding; the shader engine uses an adaptive filter |
| Filter strength | 0–100%; strength of the adaptive filter |
| Also filter before RTX HDR | Also runs the adaptive filter on the SDR picture sent to RTX HDR, at extra GPU cost |

Debanding is enabled by default to smooth visible gradient steps. Ignore Film Grain is not a general denoiser. Sharpening is our implementation, not a calibrated reproduction of every Windows NVIDIA filter value.

## HDR engines

The **Engine** list chooses what converts SDR to HDR:

- **Auto** uses RTX HDR. It switches to the shader engine if RTX HDR cannot start, fails while running, if the desktop is not rendering on an NVIDIA GPU, or if the display peak is outside 400–2,000 nits. The panel shows the engine in use and the reason. **Retry RTX HDR** tries again.
- **RTX HDR only** never uses the shader engine. A failure leaves the window unchanged and reports the error, as earlier versions did.
- **Shader only** never loads NVIDIA's engine.

If KWin stops while RTX HDR is starting, the next session in Auto stays on the shader engine until you press **Retry RTX HDR**.

Middle greys, Contrast and Saturation apply to both engines, but the same value will not look identical on each. **Shader engine tuning** holds the options that apply only to the shader engine:

| Control | Behavior |
|---|---|
| Preset | Standard, Cinema, High dynamic, Soft natural, Bright highlights or Vivid; editing a value below changes this to Custom |
| Tone curve | BT.2446 Method A follows the ITU SDR-to-HDR expansion. Highlight boost keeps SDR tones and lifts only the brightest part of the picture |
| Peak output | 25–100% of the display peak |
| Exposure | −2 to +2 stops, before tone mapping |
| Colour boost | −100% to +100%, on top of Saturation |
| Highlight start, Highlight gain | Highlight boost curve only: where the boost begins, and 0–5 stops added at SDR white |
| Shadow start, Shadow depth | Darkens tones under the start level; zero depth leaves them unchanged |

The engine and tuning values are saved in each profile. The fallback changes only the HDR conversion. Windows the direct path cannot import stay unchanged with any engine.

## Profiles

Changes save automatically. Use Save as to create a named profile and the Profile list to switch. Delete removes a custom profile and restores Global; Global itself is retained. Application approval and picture-profile selection are separate. The current implementation does not promise automatic per-application preset assignment.

Peak brightness and display selection are treated separately from reusable picture settings so presets do not overwrite display calibration.

## Other windows and screenshots

The sidebar slides in from the left and spans the full output height, including the taskbar area beneath it. Its translucent background keeps the application visible beneath the animated lightning. Hover over a control for its explanation.

Clicks outside the sidebar reach the desktop, including exposed parts of the taskbar. A slider drag stays with its control until you release it, even if the pointer leaves the sidebar. Clicking another application returns typing to it.

KDE screenshot shortcuts remain available while the sidebar is open, including Print Screen and Meta+Shift+S where configured. Spectacle can capture the sidebar and the application together. During region selection, Chaos HDR yields input and stops drawing over the selection tool; the captured image already contains the sidebar. It resumes afterward. The final game draw respects KWin clipping. Screenshot color depends on the capture tool and viewer; an SDR screenshot may not reproduce an HDR display's appearance.

An unsupported window remains unfiltered rather than switching silently to a slower capture path. Check the panel message and [compatibility requirements](compatibility.md).

## Accessibility and text editing

Open **More → Reduce motion** for a static wordmark/lightning background and
instant opening and closing. The default retains the animated Chaos HDR design.

**More → Accessible controls** opens a native Qt controls window with labelled
fields, keyboard navigation and the platform accessibility interface. It also
appears as an action on the application launcher. It includes application
approval, profiles, HDR engines, shader tuning, debanding and sharpening. It follows the desktop's text settings;
full screen-reader validation is still part of release acceptance.

Profile names support Unicode. The overlay uses installed fallback fonts for
characters absent from the brand fonts. Complex text shaping is handled by the
native controls window. In the overlay name field, Ctrl+A/C/X/V and normal
navigation keys work. Clipboard access happens only when you request copy,
cut or paste; clipboard contents are not written to a project file.

## Display changes and recovery

Move the selected application between screens and its effective brightness
follows the screen receiving the window. An unavailable or SDR output bypasses
HDR without switching off a separately enabled sharpening setting. Your saved
picture profile does not overwrite KDE's display calibration.

Unsupported translucent windows stay unchanged. A temporary processing failure
gets a small number of automatic retries. **Retry processing** can request a new
attempt; removing and allowing the application again is also available.
Disconnecting the screen hosting the sidebar closes it and releases its input
and cursor ownership. Open it again with Alt+Z on an available screen.
