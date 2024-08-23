# Widescreen Patch for [BlackMythWuKong]

## Overview

This project provides a widescreen patch for [BlackMythWuKong], allowing players to enjoy the game in a 21:9 aspect ratio. The original idea for this patch comes from the Bilibili content creator [conglin3](https://space.bilibili.com/543249142)

## Features

- Uses memory patching to modify the game process, eliminating the need for file modifications.
- No impact on normal game operation.
- Automatically launches and adjusts the game to widescreen mode.
- Precisely locates the data that needs to be modified for widescreen, requiring only a single memory modification.
- 
## How to Use

1. Run the program directly. It will launch the game and attempt to adjust it to widescreen mode.
2. If the game doesn't switch to widescreen immediately, go to the in-game settings -> Display and change the aspect ratio to 21:9. (It will comes 32:9)
3. Currently, the program can automatically launch games from Steam. For games from other sources:
   - Run the program first.
   - When prompted that the game cannot be found, manually launch the game yourself, then the program will modify the game process shortly

## Note

This program currently supports automatic launching only for Steam-sourced games. For games from other platforms or standalone installations, manual game launch is required after running this patcher.

## Credits

Original idea by [conglin3](https://space.bilibili.com/543249142) on Bilibili.
Program and in memory patcher : MJ0011 of Kunlun Lab

## Disclaimer

This software is for educational purposes only. Please ensure you have the right to modify and use the game in this manner. The authors are not responsible for any misuse or potential violations of terms of service.

## Contributing

Contributions, issues, and feature requests are welcome. Feel free to check [issues page](link-to-your-issues-page) if you want to contribute.

## License

This project is licensed under the MIT License
