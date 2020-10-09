# pbxTeleporterServers 
pbxTeleporter is the server program for the Pixel Teleporter project

A pbxTeleporter server reads pixel output from a Pixelblaze LED controller, converts it
to a network-compatible format, and sends it on request to one or more client computers.

This project contains all the source and project files to build pbxTeleporter for
various platforms. 

The currently supported hardware/software platforms are:
- **[ESP8266](./ESP8266)** 
- **[Raspberry Pi](./Pi)** (uses Pi serial input, requires 5v -> 3.3v converter)
- **[Linux](./Linux)** (requires an FTDI or similar USB-to-Serial converter)
- **[Windows](./Windows)** (requires an FTDI or similar USB-to-Serial converter)

This source is for archival and reference only.  You do not need anything from
this repository to run or build the main [Pixel Teleporter](https://github.com/zranger1/PixelTeleporter) project. 

  
