#!/usr/bin/python3

import serial.tools.list_ports
import sys


def find_arduino():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if p.description == "Arduino Micro":
            return p
    return None


def main():
    if len(sys.argv) != 5:
        print("Must have four arguments: <switch number> <red> <green> <blue>")
        sys.exit(-1)

    sw = int(sys.argv[1])
    red = int(sys.argv[2])
    green = int(sys.argv[3])
    blue = int(sys.argv[4])

    cmd_string = "COL {0:1d} {1:03d} {2:03d} {3:03d}\n".format(sw, red, green, blue)

    port = find_arduino()

    if port is None:
        print("Arduino could not be found!")
        return

    ser = serial.Serial(port.device, 115200, timeout=1)
    ser.write(cmd_string.encode())
    res = ser.readline().decode()
    ser.flush()
    ser.close()

    if not res == "OK\n":
        print("Something went wrong: "+res)
        sys.exit(-1)


if __name__ == "__main__":
    # execute only if run as a script
    main()
