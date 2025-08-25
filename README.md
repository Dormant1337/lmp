# lmp - The Linux Music Player.

[![Stars](https://img.shields.io/github/stars/Zer0Flux86/lmp?label=Stars&labelColor=%23c2c2c2&color=%23555555&style=for-the-badge)](https://github.com/Zer0Flux86/lmp/stargazers)
![Visitors](https://img.shields.io/badge/Visitors-11-%23555555?labelColor=%23c2c2c2&style=for-the-badge)
[![Made with C](https://img.shields.io/badge/Made%20with-C-blue?labelColor=%23c2c2c2&color=%23555555&style=for-the-badge)](https://en.wikipedia.org/wiki/C_(programming_language))

## Dependencies

The following libraries and utilities are required to build and run `lmp`:

### System Libraries
*   **ncurses**, **SDL2**, **SDL2_mixer**, **mpg123**, **cJSON** (included in source).

### External Utilities (for `webdownload` command)

*   **spotdl** (`yay -S spotdl` or `pip install spotdl` or `pipx install spotdl`)  
*   **yt-dlp** (install via `sudo pacman -S yt-dlp`, update via `yt-dlp -U`)  
*   **ffmpeg** (`sudo pacman -S ffmpeg`)


## Building

    gcc main.c functions.c config.c cJSON.c -o audioplayer -lncurses -lSDL2 -lSDL2_mixer -lmpg123
