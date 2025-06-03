# Project3

## Build Instructions

1. **Compile all source files in the subdirectories:**
   ```bash
   make
   ```
2. **Clean up executables:**
   ```bash
   make clean
   ```
3. **Compile and test the source code:**
   ```bash
   make test
   ```
---

## Disk

The programs simulate a HDD and work as a disk sever to read/write information.

### Run Programs

```bash
./BDS <disk file name> <cylinders> <sector per cylinder> <track-to-track delay> <port>
```
This assigns the server to `127.0.0.1` (localhost).

---

## File System

The shell program works as a sever supporting more than a client in parallel.

### Run a server

```bash
./FS <BDSPort> <FSPort>
```
This assigns the server to `127.0.0.1` (localhost).

### Run a cilent and connect to the cilent

```bash
./FC <FSPort>
```
