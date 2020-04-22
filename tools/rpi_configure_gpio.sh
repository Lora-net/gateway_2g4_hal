#!/bin/sh

# This script is intended to be used on the LoRa 2.4Ghz gateway when using a Raspberry Pi as host.
# It performs the following actions:
#       - export/unpexort GPIO23 and GPIO18 used to drive the MCU_RESET and MCU_BOOT0 pins of the gateway.
#
# Usage examples:
#       ./rpi_configure_gpio.sh stop
#       ./rpi_configure_gpio.sh start

# GPIO mapping has to be adapted with HW
#

MCU_RESET_PIN=23
MCU_BOOT0_PIN=18

WAIT_GPIO() {
    sleep 0.1
}

init() {
    # setup GPIOs
    echo "$MCU_RESET_PIN" > /sys/class/gpio/export; WAIT_GPIO
    echo "$MCU_BOOT0_PIN" > /sys/class/gpio/export; WAIT_GPIO

    # set GPIOs as output
    echo "out" > /sys/class/gpio/gpio$MCU_RESET_PIN/direction; WAIT_GPIO
    echo "in" > /sys/class/gpio/gpio$MCU_BOOT0_PIN/direction; WAIT_GPIO

    echo "Driving MCU_RESET pin through GPIO$MCU_RESET_PIN..."
    echo "Driving MCU_BOOT0 pin through GPIO$MCU_BOOT0_PIN..."

    # drive the pins
    echo "1" > /sys/class/gpio/gpio$MCU_RESET_PIN/value; WAIT_GPIO
}

term() {
    # cleanup all GPIOs
    if [ -d /sys/class/gpio/gpio$MCU_RESET_PIN ]
    then
        echo "$MCU_RESET_PIN" > /sys/class/gpio/unexport; WAIT_GPIO
    fi
    if [ -d /sys/class/gpio/gpio$MCU_BOOT0_PIN ]
    then
        echo "$MCU_BOOT0_PIN" > /sys/class/gpio/unexport; WAIT_GPIO
    fi
}

case "$1" in
    start)
    term # just in case
    init
    ;;
    stop)
    term
    ;;
    *)
    echo "Usage: $0 {start|stop}"
    exit 1
    ;;
esac

exit 0
