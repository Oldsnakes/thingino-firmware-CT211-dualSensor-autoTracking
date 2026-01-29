1/28/2026

This project is based on thingino-firware and Prudynt-T.   Some features been added to the Prudynt-T and WebUI for my own  experiment.  
It was based on the “master+eebcb12” build with Oct. 2025 JASON WebUI.  And, it has only been tested on <szt_ct211_t23n_gc1084_dual_atbm6012bx>, 
which is not a currently supported platform, therefore it is nowhere near completed. 

The Thingino-firmware development has already evolved a lot on WebUI by the team.  Therefore these changes no longer able to merge with the current repository.  
Since I am not following all the improvements in the current development closely, some of these may already have been worked on or discussed.  
Therefore, I would like to just post them as ideas for the team to consider as features to be added, or as alternatives. 

-  Add control to WebUI for manual exposure and analog gain to extend low light range.
-  Integrate GPIO control into Prudynt for better access to the lights, sensor switch.  
-  Add Tiled/Map multi-ROIs to motion control.  This allows detection of regions of interest only to avoid false alarm.
-  Integrate Motor control to interface with motor-daemon for faster response.
-  Add auto tracking to PTZ camera and turn on white light when motion is detected.  
-  Add dual-sensor/dual-stream (time-shared) RTSP/JPEG streaming at front end and muli-view to WebUI.  (platform dependent)
![preview-Dual Sensor](https://github.com/user-attachments/assets/5e6954fc-dde2-4d1c-af9c-e4d798c601a6)
![streamer-Dual Sensor](https://github.com/user-attachments/assets/004868f7-e9c8-4207-929b-ca6835e11c7d)
![Motion-Box Mode](https://github.com/user-attachments/assets/151f6293-5d9c-46f2-b67a-dffb94f928cb)
![Motion-Map Mode](https://github.com/user-attachments/assets/907a0b99-2fdc-4f38-a552-897d415c25d3)
![Motion-heat map](https://github.com/user-attachments/assets/ba893a61-6964-4817-8d8c-0977f1bbf2d0)
