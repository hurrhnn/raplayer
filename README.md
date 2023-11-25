# raplayer
[![Build Status](https://jenkins.hurrhnn.xyz/job/raplayer/badge/icon)](https://jenkins.hurrhnn.xyz/job/raplayer/)
<br><br>The raplayer is a C-based cross-platfrom remote audio player.<br> It can play simple audio files or streams.<br>

![image](https://user-images.githubusercontent.com/40728528/166156129-60f3a516-3f13-4391-91f9-f4166f5615f7.png)

## Getting Started

### Coverage
```
Windows: Cygwin environment only tested, MSVC and MinGW does NOT supported yet.
Linux: supported.
MacOS: supported.
```

### dependencies
Windows(cygwin): `cmake make gcc-core`<br>
Linux: `alsa-lib-development-package(debian: libasound-dev, RHEL: alsa-lib-devel) cmake make gcc`<br>
MacOS: `cmake make gcc`<br>

Optional packages: `libopus portaudio`

### Building
run `build.sh` to build, or configure cmake manually. cmake will automatically build optional packages and build the source.

#### Cmake Options
`-DBUILD_SHARED_LIBS`, Build using shared libraries.<br>
`-DRAPLAYER_BUILD_DEPS`, Try to build core dependencies.<br>
`-DRAPLAYER_DISABLE_APPLICATION`, Disable to build raplayer cli application.<br>
`-DRAPLAYER_INSTALL_PKGCONFIG`, install pkg-config module.

## Running CLI application

The `server` mode is audio provider mode, `client` mode is audio player mode. <br>
raplayer can run `server`, `client` mode with one executable.

```bash
$ ./main

Usage: ./main <Running Mode>

--client: Running on client mode.
--server: Running on server mode.

```

### Server mode
```bash
$ ./main --server

Usage: ./main --server [--stream] <FILE> [Port]

<FILE>: The name of the pcm_s16le wav file to play. ("-" to receive from STDIN)
[--stream]: Allow consume streams from STDIN until client connected. (DEPRECATED)
[Port]: The port on the server to which you want to open.

```

### Client mode
```bash
$ ./main --client

Usage: ./main --client <Server Address> [Port]

<Server Address>: The IP or address of the server to which you want to connect.
[Port]: The port on the server to which you want to connect.

```
**You can adjust the volume by pressing the up and down arrow keys during playback.**

### Usage examples

- Play pcm_16le wav file.
```bash
./main --server s16le.pcm
```

- Play audio file using `ffmpeg`.
```bash
ffmpeg -loglevel panic -i audio.mp3 -f s16le -ac 2 -ar 48000 -acodec pcm_s16le - | ./main --server -
```

- Play youtube audio using `ffmpeg` & `yt-dlp`.
```bash
yt-dlp --quiet -f bestaudio "[Youtube URL]" -o - | ffmpeg -loglevel panic -i pipe: -f s16le -ac 2 -ar 48000 -acodec pcm_s16le - | ./main --server -
```

- Play audio stream using `ffmpeg`.
```bash
ffmpeg -loglevel panic -i http://aac.cbs.co.kr/cbs939/_definst_/cbs939.stream/playlist.m3u8 -f s16le -ac 2 -ar 48000 -acodec pcm_s16le - | ./main --server --stream -
```
## Known issues

- There is a slight difference in playback time between clients when connecting multiple clients.

## License

This project is licensed under the GPLv3 License - see the [LICENSE](https://github.com/hurrhnn/raplayer/blob/main/LICENSE) file for details.

## Contributions

PR is always welcome. If you have any questions, please contact at 20sunrin022@sunrint.hs.kr.
