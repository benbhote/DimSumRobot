# DimSumRobot

This is a personal project of a miniature robot based on a ESP32-S3 microchip.

The original aim was to have a RC robot (from a webpage) with the possibility to plug it on an Astronomical telescope Hadley (more about it here : https://www.printables.com/model/224383-astronomical-telescope-hadley-an-easy-assembly-hig).
But with the actual focuser on my Hadley, the camera sensor is too far from the mirror (I need to explore a solution based on a Barlow lens to solve this problem).

Nonetheless, the project is still really fun because of all the other dimensions of the robot :
- Movement based on two wheels only (self-balancing)
- Measuring distance with an Ultrasonic radar, and ability to make emergency stop
- Reading of temperature and humidity
- Capturing pictures or streaming while in motion
- Managing the SD card (downloading and deleting pictures)

# Known issues :
- Currently, the self-balancing isn't properly working and the robot will move with its bottom touching the floor.
- If the robot is on for extended period of time (more than 30min I'd say), the warmth emit by the ESP32-S3 is building up inside the chassis and mainly going out through the SHT40 slit and impacting the data read by it.
- Depending on how well the wheels are fixed, the robot will not go straight forward.

# Next Step :
- Making the PID loop working properly
- Adding conditional compilation to adapt the code depending on the components inside the robot
