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
    if len(sys.argv) != 2:
        print("Must have two arguments: <dim>")
        sys.exit(-1)

    dim = int(sys.argv[1])

    cmd_string = "DIM {:03d}\n".format(dim)

    port = find_arduino()

    if port is None:
        print("Arduino could not be found!")
        return

    ser = serial.Serial(port.device, 115200, timeout=1)
    ser.write(cmd_string.encode())
    res = ser.readline().decode()
    ser.close()

    if not res == "OK\n":
        print("Something went wrong: "+res)
        sys.exit(-1)


if __name__ == "__main__":
    # execute only if run as a script
    main()
