# directcapture
## Directly records sound from applications.

This command-line tool for Windows 64-bit records another application's DirectSound output into a wave file.
Note that immediately after hooking into an application the application's sound engine has to be reinitialized to ensure it will in fact record to the output file. Since DLL injection is used, antivirus-softwares will go nuts. 

### Libraries used
* [cxxopts](https://github.com/jarro2783/cxxopts)
