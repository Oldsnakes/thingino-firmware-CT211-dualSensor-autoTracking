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
![preview-Dual Sensor-1](https://github.com/user-attachments/assets/66fbfdaf-b6a1-4053-81ec-bd5e3e5cacae)
![streamer-Dual Sensor-1](https://github.com/user-attachments/assets/d49fc930-2191-47fc-9a3b-3d4105ad5dc7)
![Motion-Box Mode-1](https://github.com/user-attachments/assets/3cfd0f67-4549-4b62-94eb-99277ef237dd)
![Motion-Map Mode-1](https://github.com/user-attachments/assets/991bdb29-53c2-41dc-b2bb-a76220bfed4d)
![Motion-heat map-logread](https://github.com/user-attachments/assets/1c235d8a-e411-4478-8137-e6d84a47b898)
