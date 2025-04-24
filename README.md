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
