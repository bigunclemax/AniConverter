# Ford SYNC 3 animations converter

These tools allow you to convert animations files (hereinafter .ani) from proprietary format to raw h264 bitstream and back.   
Then h264 files could be easily converted to any popular video format with ffmpeg tools for example.  

## How to
### .ani to .mp4
```shell
ani2h264raw IN.ani ./out.h264
ffmpeg -i out.h264 -c copy out.mp4
```

### .mp4 to .ani
```shell
ffmpeg -i IN.mp4 -c:v libx264 -aud 1 -color_primaries 1 -color_trc 1 -colorspace 1 -filter:v scale=800x480,setsar=1/1,setdar=5/3 -f rawvideo out.h264
h264raw2ani out.h264 ./out.ani
```