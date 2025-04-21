import argparse
import lgpio
import time

# Define GPIO pin for the new functionality
GPIO_PIN = 2

# Set up argument parsing
parser = argparse.ArgumentParser(description="Control GPIO pins.")
parser.add_argument("--on", action="store_true", help="Turn GPIO 2 on.")
parser.add_argument("--off", action="store_true", help="Turn GPIO 2 off.")
parser.add_argument("--cycle", action="store_true", help="Cycle GPIO 2: turn it off for a second, then back on.")
args = parser.parse_args()

# Open the GPIO chip and set the GPIO_PIN as output
h = lgpio.gpiochip_open(0)
lgpio.gpio_claim_output(h, GPIO_PIN)

try:
    if args.on:
        # Turn GPIO_PIN on
        lgpio.gpio_write(h, GPIO_PIN, 1)
        print("GPIO 2 is ON")
    elif args.off:
        # Turn GPIO_PIN off
        lgpio.gpio_write(h, GPIO_PIN, 0)
        print("GPIO 2 is OFF")
    elif args.cycle:
        # Cycle GPIO_PIN: off for a second, then back on
        lgpio.gpio_write(h, GPIO_PIN, 0)
        print("GPIO 2 is OFF for 1 second")
        time.sleep(1)
        lgpio.gpio_write(h, GPIO_PIN, 1)
        print("GPIO 2 is ON again")
    else:
        print("Please specify --on, --off, or --cycle to control GPIO 2.")
finally:
    # Clean up
    lgpio.gpiochip_close(h)
