# File configuration steps
MultiWell Robot MicroManager and Arduino Scripts

Recommended steps for independent/modular robot automation:

1. Visit Arduino Create to upload RobotController_2018.ino script to Arduino(1):
https://create.arduino.cc/editor/rosslagoy/33ba4f04-3064-49b6-8265-8dfdec810b8c
2. Use MATLAB to control the robotic system with ROBOT_2018.m
3. Or, use serial communication with RobotController_2018.ino through Arduino Create.

Recommended steps for integrated robot with microscopy automation:

1. Visit Arduino Create to upload the Arduino scripts to each respective controller:
https://create.arduino.cc/editor/rosslagoy/33ba4f04-3064-49b6-8265-8dfdec810b8c
2. Follow README.md steps at (below) to set up MicroManager (.bsh) scripts:
https://github.com/albrechtLab/MicroManager

An interactive heatmap visulization of chemical screen data is also available online, with code in this repo (hmv.tsv, hmv.html):
http://www.rosslagoy.com/hmv.html

CAD of the multiwell robot (Fig. 1, Supp. Fig. 1) will be made available online.

Reference:
Lagoy, R.C., Albrecht, D.R. (2018). “Automated fluid delivery from multiwell plates to microfluidic devices for high-throughput experiments and microscopy.” Scientific Reports. 8, 6217. doi:10.1038/s41598-018-24504-x

https://www.nature.com/articles/s41598-018-24504-x
