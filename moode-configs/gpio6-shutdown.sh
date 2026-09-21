#!/bin/bash

/usr/bin/gpiomon --chip gpiochip0 --edges rising --num-events 1 GPIO6

/usr/bin/systemctl poweroff 
