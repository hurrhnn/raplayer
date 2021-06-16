# raplayer
The raplayer is a C-based cross-platfrom remote audio player.<br> It can play simple files or streams.

## Getting Started

### Coverage
```
Windows: cygwin environment only tested, MSVC NOT supported yet.
Linux: supported.
MacOS: supported.
```

### Prerequisites

#### You must pre-install the corresponding package for each OS.

Windows: `cmake make gcc`<br>
Linux: `alsa lib development package(debian: libasound-dev, RHEL: alsa-lib-devel) cmake make gcc`<br>
MacOS: `cmake make gcc`<br>

Optional package: `ffmpeg`

### Building
When you run the script, cmake automatically downloads the submodule and builds the source.

```bash
chmod +x ./build.sh && ./build.sh
```

## Running the raplayer

`server` mode is an audio provider mode, `client` mode is an audio player mode. <br>
The raplayer can set `server`, `client` mode with one executable file.

```bash
$ ./raplayer 

Usage: ./raplayer <Running Mode>

--client: Running on client mode.
--server: Running on server mode.

```

### Server mode
```bash
$ ./raplayer --server

Usage: ./raplayer --server [--stream] <FILE> [Port]

<FILE>: The name of the wav file to play. ("-" to receive from STDIN)
[--stream]: Allows flushing STDIN pipe when client connected. (prevent stacking buffer)
[Port]: The port on the server to which you want to open.

```

### Client mode
```bash
$ ./raplayer --client

Usage: ./raplayer --client <Server Address> [Port]

<Server Address>: The IP or address of the server to which you want to connect.
[Port]: The port on the server to which you want to connect.

```

### Usage examples

 - Play the pcm_16le file.
```bash
./raplayer --server s16le.pcm
```

 - Play audio file using ffmpeg.

```bash
ffmpeg -i audio.mp3 -f s16le -ac 2 -ar 48000 -acodec pcm_s16le - | ./raplayer --server -
```

 - Play audio **stream** using ffmpeg.

```bash
ffmpeg -i http://aac.cbs.co.kr/cbs939/_definst_/cbs939.stream/playlist.m3u8 -f s16le -ac 2 -ar 48000 -acodec pcm_s16le - | ./raplayer --server --stream -
```
## Known issues

 - When building a source from MacOSX to a build script, it is not built at once (must be done two times).
 - When running in pipe mode from server mode, pcm data length is not displayed as "STDIN" when connecting clients in some environments.

## License

This project is licensed under the GPLv3 License - see the [LICENSE.md](https://github.com/hurrhnn/raplayer/blob/main/LICENSE) file for details.

## Contributions

PR is always welcome. If you have any questions, please contact at 20sunrin022@sunrint.hs.kr.
