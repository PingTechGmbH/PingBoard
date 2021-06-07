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
    if len(sys.argv) == 6:
        r =  int(sys.argv[3])
        g =  int(sys.argv[4])
        b =  int(sys.argv[5])
    elif len(sys.argv) == 3 and sys.argv[2] == 'OFF':
        r =  0
        g =  0
        b =  0
    elif len(sys.argv) != 6:
        print("Must have five arguments: <switch> <mode> <red> <green> <blue>")
        sys.exit(-1)

    sw = int(sys.argv[1])
    mode =   sys.argv[2]

    cmd_string = "BLNK {:1d} {} {:03d} {:03d} {:03d}\n".format(sw, mode, r, g, b)

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
