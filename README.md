# lmp - The Linux Music Player.

[![Stars](https://img.shields.io/github/stars/Zer0Flux86/lmp?label=Stars&labelColor=%23c2c2c2&color=%23555555&style=for-the-badge)](https://github.com/Zer0Flux86/lmp/stargazers)
![Visitors](https://img.shields.io/badge/Visitors-11-%23555555?labelColor=%23c2c2c2&style=for-the-badge) <!-- Или упрощенный URL выше -->
[![Made with C](https://img.shields.io/badge/Made%20with-C-blue?labelColor=%23c2c2c2&color=%23555555&style=for-the-badge)](https://en.wikipedia.org/wiki/C_(programming_language))

## Building

    gcc main.c functions.c config.c cJSON.c -o audioplayer -lncurses -lSDL2 -lSDL2_mixer -lmpg123
