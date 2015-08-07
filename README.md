# ffvi
wrapper of ffmpeg to record videos in labview

##Usage
1. Before grabbing frames, invoke `ff init.vi` to set up environment for a new video file.
2. Send an IMAQ image to `ff wframe` to write a frame into the file.
3. After grabbing, invoke `ff close` to finish the video file.

##Limitations
Only support one recording instance per process.
