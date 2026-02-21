# Introduction
This code is used for CROC (Combat Robotics of Carleton) Scrap-Bots combat event. This uses xbox controllers to drive combat robots that consist of an ESP32 used to controll 2 drive motors and a weapon motor. 

# Notes
Follow correct power-on and safety procedures to ensure safe opperation of the robots and this code. 

# Controls
In order to connect controller, type mac address into the '#define ALLOWED_CONTROLLER' line in the 'main.cpp' file. Next power on robot and place controller in pairing mode. 

The onboard led will blink rapidly and turn solid when paired succesfully. 

Press both the right and left bumper at the same time and release to enable robot. 

Press both bumpers to disable. 

Drive with the right joystick and opperate weapon speed with the left trigger. 