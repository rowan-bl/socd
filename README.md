# Last Input Priority SOCD Cleaner

This project is a streamlined SOCD cleaner optimized for FPS games. It is a recoded version of the original [SOCD Cleaner](https://github.com/valignatev/socd) by [valignatev](https://github.com/valignatev), with unnecessary features removed to focus on enhancing performance in FPS games.

## Compilation

Using MSVC

```sh
cl socd_cleaner.c resource.res user32.lib shlwapi.lib gdi32.lib /Fesocd.exe
```
   
## Custom Configurations
The config file named (`socd.conf`) is in the same directory as the exe, you may need to run the exe to create the file.
- The config file uses [virtual key codes](https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes), omitting the 0x prefix (0x51 would be input as 51).
- The order in the config file is a1, a2, b1, b2, c1, c2, disable key, with each letter interacting with each other.

## Acknowledgements
Once again, special thanks to [valignatev](https://github.com/valignatev) for creating the original SOCD Cleaner.
