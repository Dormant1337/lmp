# lmp - The Linux Music Player.

[![Stars](https://img.shields.io/github/stars/Zer0Flux86/lmp?label=Stars&labelColor=%23c2c2c2&color=%23555555&style=for-the-badge)](https://github.com/Zer0Flux86/lmp/stargazers)
![Visitors](https://img.shields.io/badge/Visitors-11-%23555555?labelColor=%23c2c2c2&color=%23555555&style=for-the-badge)
[![Made with C](https://img.shields.io/badge/Made%20with-C-blue?labelColor=%23c2c2c2&color=%23555555&style=for-the-badge)](https://en.wikipedia.org/wiki/C_(programming_language))

## Dependencies

The following libraries and utilities are required to build and run `lmp`:

### System Libraries
*   **ncurses**, **SDL2**, **SDL2_mixer**, **mpg123**, **cJSON** (included in source).
    *   **Arch/Manjaro:** `sudo pacman -S ncurses sdl2 sdl2_mixer mpg123`
    *   **Ubuntu/Debian:** `sudo apt install libncurses-dev libsdl2-dev libsdl2-mixer-dev libmpg123-dev`

### External Utilities (for `webdownload` command)
*   **spotdl** (`yay -S spotdl` or `pip install spotdl` or `pipx install spotdl`)
*   **yt-dlp** (install via `sudo pacman -S yt-dlp`, update via `yt-dlp -U`)
*   **ffmpeg** (`sudo pacman -S ffmpeg`)

## Building on arch through aur:

    yay -S lmplayer

# Building on other os:
    git clone https://github.com/Zer0Flux86/lmp
    cd lmp
    sudo make install
    lmplayer

## Usage

Start the player by running:

    lmplayer

Press `:` to enter command mode, then type `help` to see a list of available commands.
You can also press `q` to quickly exit the player.

## Configuration

`lmp` saves its state (library, playlists, current volume, last played track) to a JSON file located at:

    ~/.config/LMP/config.json

## Contributing

Contributions are welcome! Feel free to open issues or submit pull requests.
Please ensure that all contributions adhere to the project's existing code style and architectural principles.

## Authors

*   **dormant1337:** [https://github.com/Zer0Flux86](https://github.com/Zer0Flux86)
*   **Syn4pse:** [https://github.com/BoLIIIoi](https://github.com/BoLIIIoi)
