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
| `L` | Keep my choice on or off |
| `?` | Show or hide shortcuts |
| `Q` | Quit |

## Command line

```sh
iconic status | list-themes | apply <theme> [--lock|--follow] | lock | follow   # all take --json
iconic install <archive|folder>... [--replace|--keep-both] [--apply]
iconic shell-icons                                                          # rebuild launcher/dock icons
```

## Build

```sh
bin/build && build/iconic
bin/test
bin/install   # makepkg -fsi, adds Iconic to the launcher
```
