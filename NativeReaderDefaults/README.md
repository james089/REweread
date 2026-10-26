# Native reader contrast default

On verified firmware `20260827113527`, native document loading calls
`QJsonObject::value("viewBackgroundFilter").toString()`. Both an absent field
and an explicit empty string become Adaptive. Off is `"off"`; Full page is
`"fullpage"`. `[Dialogs] BackgroundFilter` only records onboarding completion.

This independent XOVI extension and QMD hook inspect the current document's
`.content` when the native `documentOpened` event fires. For PDF/EPUB files
with an absent field and an empty live filter, the hook calls the native
`DocumentController.setViewBackgroundFilter(id, "off")`. The native controller
owns persistence. Explicit values, including the empty Adaptive value, are
preserved. Notebooks, malformed/unreadable metadata and unknown types are left
alone. The reader default takes effect when a document is next opened; there
is no bulk rewrite or background scan.

The helper only reads one metadata file; it does not read book contents,
accounts, or WeRead data. It does not change the WeRead plugin or existing
reading-enhancements QMD. No system binary is patched. Installation and a
pre-start guard gate the exact OS, Xochitl SHA-256 and resource table. An
incomplete installation disables this patch through a rollback timer.

Build with the official SDK using `bash NativeReaderDefaults/build-sdk.sh`.
`native_reader_defaults_test` takes
the absolute path of `defaults.qmd` and exercises metadata policy and the
actual inserted QML handler using synthetic files and a fake controller.
`native_reader_defaults_smoke` takes the built module path and checks its
exported registration and QML singleton without reading the system library.

## Flat reader menu

`menu.qmd` enables the native SettingsMenu adjustment loader directly and hides
the outer AdjustmentToolsMenu. Existing FilterMenu and TextSettingsButton
components retain their native visibility rules and event handlers. Reader
SearchButton and ShareMenu are hidden with `shouldShow: false`, which also
collapses their layout slots. This does not hide the library's search control.
The separate PDF fit/crop submenu remains available as “页面缩放”.

For an existing verified defaults installation, `install-menu-device.sh` adds
only the menu QMD and extends the firmware-update guard to cover it. It checks
the current installation manifest, saves the old guard/manifest and arms
`rollback-menu.sh` before activating the menu. Restart Xochitl separately after
the preparation command returns. Mark `.menu-install-ok` with the installation
stamp and cancel the matching timer only after the menu QML readiness marker
and existing extensions have been confirmed. No root filesystem remount or
whole-device reboot is needed.
