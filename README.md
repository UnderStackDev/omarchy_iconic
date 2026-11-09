# Iconic

Icon themes and app launchers for [Omarchy](https://omarchy.org).

![Iconic picking an icon theme](docs/screenshot.png)

Iconic does three things, from one keyboard-driven window that follows your
Omarchy theme:

- **Themes** — pick the desktop's icon theme from previews of every installed
  theme. In **Follow** mode your pick stays until you switch to another Omarchy
  theme, which then picks its own; with **Keep my choice** (Lock) it stays for
  good. Either way it survives `omarchy update`.
- **Install** — drop an icon theme archive or folder onto the window. It lands
  in `~/.local/share/icons`, where Flatpak apps can see it too.
- **Apps** — rename a launcher, give it another icon, or hide it, without
  touching the package's own files.

## Why app icons finally follow the theme

Omarchy's launcher and dock don't look app icons up in the active icon theme.
They index every app icon from every installed theme and keep the first file
they find per name, SVGs before PNGs. So switching the icon theme usually
changes folders but not apps, and PNG themes such as Yaru never show up in the
launcher at all.

Iconic adds a small icon layer at `~/.icons/iconic-apps` that the launcher
reads first: one SVG per launcher, taken from the chosen theme (PNG icons are
wrapped in SVG so they win), or the app's own icon when the theme has none.
The layer is rebuilt whenever the icon theme or a launcher changes, and the
launcher picks it up the next time its menu or app grid opens.

## Install

Iconic is built like Omarchy's own apps: C++ and Qt Quick, one binary,
installed as an Arch package.

```sh
git clone https://github.com/UnderStackDev/omarchy_iconic.git
cd omarchy_iconic
bin/install          # builds and installs the package (makepkg -fsi)
```

Then open **Iconic** from the app launcher (`Super + Space`).

The package and command are called `omarchy_iconic`. Before 0.2.0 they were
called `iconic`, which is also the name of an unrelated GNOME app in the AUR,
so `omarchy update` replaced this app with that one
([#2](https://github.com/UnderStackDev/omarchy_iconic/issues/2)). Installing
0.2.0 removes an old `iconic` 0.x package and its hooks and keeps your
settings. If the update already put the GNOME app in its place and you don't
want it, remove it with `sudo pacman -R iconic`.

Needs `qt6-base`, `qt6-declarative`, `qt6-wayland`, `qt6-svg`,
`xdg-desktop-portal`, `libarchive` (bsdtar), `gtk-update-icon-cache` and
`desktop-file-utils`; Omarchy has them all.

## Keys

| Key | Action |
|---|---|
| `1` `2` `3` | Themes, Install, Apps |
| Arrows | Pick a theme or an app |
| `Space` / `Enter` | Apply the picked theme |
| `/` | Search |
| `L` | Keep my icon theme across Omarchy theme switches, on or off |
| `Del` | Remove the picked theme you installed |
| `O` | Choose theme files to install (Install tab) |
| `Enter` | Edit the picked app's name (Apps tab) |
| `I` | Pick an icon from the theme (Apps tab) |
| `Ctrl+S` | Save the launcher (Apps tab) |
| `Ctrl+Z` / `Ctrl+Shift+Z` | Undo / redo launcher changes |
| `A` | About |
| `?` | All keys |
| `Q` | Quit |

## Command line

Everything also works without the window. Every command takes `--json` and
exits 0 or 1.

```sh
omarchy_iconic status
omarchy_iconic list-themes
omarchy_iconic apply Papirus-Dark [--lock|--follow]
omarchy_iconic lock | follow
omarchy_iconic install ~/Downloads/theme.tar.xz [--replace|--keep-both] [--apply]
omarchy_iconic app list [--modified|--hidden]
omarchy_iconic app set firefox --name "Web" --icon web-browser [--hide|--show]
omarchy_iconic app reset firefox
omarchy_iconic shell-icons        # rebuild the launcher icon layer
```

## What it changes

| Where | What |
|---|---|
| gsettings `org.gnome.desktop.interface icon-theme` | the icon theme (GTK apps, and Flatpak apps through the settings portal) |
| `~/.config/gtk-3.0/settings.ini`, `gtk-4.0/settings.ini`, `kdeglobals`, `qt5ct`/`qt6ct` (if present) | the same theme name, one key each, everything else kept |
| `~/.config/omarchy/hooks/theme-set.d/omarchy_iconic` | runs after an Omarchy theme switch or refresh: keeps your theme (Lock, or Follow while the Omarchy theme is the same), or takes the new Omarchy theme's pick and copies it to the files above |
| `~/.config/omarchy/hooks/post-update.d/omarchy_iconic` | runs after `omarchy update`: puts back launcher changes that the update re-copied, rebuilds changed packages' launchers with your changes, then the theme as above |
| `~/.config/iconic/config.toml` | Follow or Lock, your theme, and the Omarchy theme it was picked under |
| `~/.config/iconic/apps.json` | your launcher changes, so they can be put back after updates |
| `~/.icons/iconic-apps/` | the launcher icon layer |
| `~/.local/share/icons/` | themes you install |
| `~/.local/share/applications/` | launcher changes: a copy marked `X-Iconic-Override=true` for system and Flatpak apps; your own launchers are edited in place after a backup in `~/.local/share/iconic/backups/` |
| `~/.local/share/iconic/icons/` | icon files you picked for launchers |

Archives are listed before they're unpacked (2 GiB / 300,000 files at most),
unpacked by bsdtar, and only files and links that stay inside the archive are
installed.

## Uninstall

```sh
sudo pacman -R omarchy_iconic
rm -rf ~/.icons/iconic-apps ~/.config/iconic \
       ~/.config/omarchy/hooks/theme-set.d/omarchy_iconic ~/.config/omarchy/hooks/post-update.d/omarchy_iconic
```

Launcher changes stay until you reset them (or delete the files marked
`X-Iconic-Override=true` in `~/.local/share/applications`).

## Build and test

```sh
bin/build && build/omarchy_iconic
bin/test
```

## License

MIT © 2026 UnderStackDev. The theme and translator code follows Omacut and
Monologue (MIT, David Heinemeier Hansson).
