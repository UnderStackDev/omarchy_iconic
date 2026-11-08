# Iconic

Icon themes and app launchers for Omarchy: pick the icon theme and keep it
across Omarchy theme switches, drop in new themes that Flatpak apps can see,
and rename or re-icon single apps. Work in progress — see `PLAN.md`.

## Shortcuts

| Key | Action |
|---|---|
| `1` `2` `3` | Switch tab |
| Arrows | Pick a theme |
| `Space` / `Enter` | Apply the picked theme |
| `/` | Search themes |
| `Del` | Remove the picked user theme |
| `O` | Choose theme files to install (Install tab) |
| `Enter` | Edit the picked app's name (Apps tab) |
| `I` | Pick an icon from the theme (Apps tab) |
| `Ctrl+S` | Save the launcher (Apps tab) |
| `Ctrl+Z` / `Ctrl+Shift+Z` | Undo / redo launcher changes |
| `L` | Keep my choice on or off |
| `?` | Show or hide shortcuts |
| `Q` | Quit |

## Command line

```sh
iconic status | list-themes | apply <theme> [--lock|--follow] | lock | follow   # all take --json
iconic install <archive|folder>... [--replace|--keep-both] [--apply]
iconic app list | app set <id> [--name N] [--icon I] [--hide|--show] | app reset <id>
iconic shell-icons                                                          # rebuild launcher/dock icons
```

## Build

```sh
bin/build && build/iconic
bin/test
bin/install   # makepkg -fsi, adds Iconic to the launcher
```
