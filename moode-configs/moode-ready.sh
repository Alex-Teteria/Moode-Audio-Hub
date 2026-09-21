#!/bin/bash

while ! curl -fsS -o /dev/null http://localhost/; do
    sleep 1
done

exec /usr/bin/gpioset --chip gpiochip0 23=1
