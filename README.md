install these packages
```bash
sudo apt install make git clang libssl-dev openssl
```

make the binaries
```bash
make
```

run the desired binaries at enough privilege to access ```/dev/input/eventX``` and ```/dev/uinput```, e.g.
```bash
sudo ./client
```
or
```bash
sudo ./server
```


# Writing macros

## Commands:
press BUTTON

release BUTTON 

-- Available buttons below

wait time 

-- time is expressed as a positive integer representing time to wait in milliseconds

joy_l [x, y]

-- [x, y] x and y are both floats in the range -1.0 to 1.0 representing the left joystick position (note that square brackets are mandatory)

joy_r [x, y]

-- [x, y] same as above but for the right joystick

play [macro, ...]

-- [marco, ...] list of strings representing the barcodes the play command can play, one will be randomly selected each time the command is ran. At least one macro must be specified. Square brackets mandatory.

## Available buttons
0
1
2
3
4
5
6
7
8
9

A
B
X
Y

TL
TR
TL2
TR2

START
SELECT

THUMBL
THUMBR

DPAD_UP
DPAD_DOWN
DPAD_LEFT
BTN_DPAD_RIGHT
