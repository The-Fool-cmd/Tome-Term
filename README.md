# Tome-Term

A terminal-based personal book library with a SQLite backend. Track, rate, review, and filter your reading list without leaving the terminal.

## Features

- Browse your library in a scrollable two-pane interface
- Add, edit, and delete book entries
- Record title, author, genres, page count, rating, completion, and a free-text review
- Filter by genre, minimum rating, or minimum length
- Persistent storage in a local SQLite database

## Requirements

- A C compiler (`gcc`)
- [ncurses](https://invisible-island.net/ncurses/)
- [SQLite3](https://www.sqlite.org/)
- A terminal at least 64×24 with colour support

On Debian/Ubuntu:

```sh
sudo apt install build-essential libncurses-dev libsqlite3-dev
```

On Fedora:

```sh
sudo dnf install gcc make ncurses-devel sqlite-devel
```

On Arch Linux:

```sh
sudo pacman -S base-devel ncurses sqlite
```

On NixOS, the included flake provides a development shell with the full
toolchain (compiler, ncurses, sqlite, make) plus debugging tools:

```sh
nix develop
```

## Building

```sh
make
```

This produces the `tome.bin` executable. Run `make clean` to remove build artifacts.

## Installing

To run Tome from anywhere, copy the binary into a directory on your `PATH`.

For the current user (no root required), using `~/.local/bin`:

```sh
mkdir -p ~/.local/bin
cp tome.bin ~/.local/bin/tome
```

If `~/.local/bin` isn't already on your `PATH`, add this to your shell's
startup file (`~/.bashrc`, `~/.zshrc`, etc.):

```sh
export PATH="$HOME/.local/bin:$PATH"
```

For a system-wide install (requires root):

```sh
sudo cp tome.bin /usr/local/bin/tome
```

You can then launch it from any directory:

```sh
tome
```

> **NixOS note:** the binary links against ncurses and sqlite from the dev
> shell's store paths, so it runs outside `nix develop` as long as those paths
> remain. For a durable, reproducible install, package Tome as a derivation and
> expose it as a flake `packages` output rather than copying the binary by hand.

## Usage

```sh
./tome.bin
```

The database is created automatically at `$HOME/Tome/tome.db` on first run.

### Key bindings

**List view**

| Key            | Action                       |
| -------------- | ---------------------------- |
| `↑` / `↓`      | Navigate the list            |
| `Enter` / `→`  | Open the selected book       |
| `n`            | Add a new book               |
| `Backspace`    | Delete the selected book     |
| `f`            | Filter the list              |
| `q`            | Quit                         |

**Details view**

| Key            | Action                       |
| -------------- | ---------------------------- |
| `↑` / `↓`      | Move between fields          |
| `Enter` / `→`  | Edit the selected field      |
| `←`            | Return to the list           |

While editing a field, `Enter` confirms and `Esc` cancels.

## Project layout

| Path              | Responsibility                                       |
| ----------------- | ---------------------------------------------------- |
| `src/master.c`    | Entry point; wires the database to the TUI           |
| `src/tui.c`       | ncurses front end and event loops                    |
| `src/database.c`  | SQLite persistence layer                             |
| `src/model.c`     | `Book` record lifecycle helpers                      |
| `include/`        | Public headers for each module                       |

## License

See repository for license details.
