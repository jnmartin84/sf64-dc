## 0. Setup Dreamcast tooling

sh4-elf toolchain (gcc 14.2 is tested), KOS v2.2.1, kos-ports/libGL all need built and installed.

For the `environ.sh` , modify the "optimization level" section like this

`export KOS_CFLAGS="${KOS_CFLAGS} -O3 -flto=auto -ffat-lto-objects"`

then

`source /opt/toolchains/dc/kos/environ.sh` 

and then...

## 1. Install build dependencies

#### Windows

For Windows 10, install WSL and a distribution by following this
[Windows Subsystem for Linux Installation Guide](https://docs.microsoft.com/en-us/windows/wsl/install-win10).
We recommend using Debian or Ubuntu 22.04 Linux distributions.

#### Linux (Native or under WSL / VM)

The build process has the following package requirements:

* make
* git
* build-essential
* python3
* pip3
* libpng-dev

Under Debian / Ubuntu (which we recommend using), you can install them with the following commands:

```bash
sudo apt update
sudo apt install make cmake git build-essential python3 python3-pip clang-format-14 clang-tidy clang-tools
```

#### MacOS

Install [Homebrew](https://brew.sh) and the following dependencies:
```
brew update
brew install coreutils make pkg-config
```
## 2. go into the repo

```bash
cd sf64-dc
```

## 3. Install python dependencies

The build process has a few python packages required that are located in `/tools/requirements-python.txt`.

To install them simply run in a terminal:

```bash
python3 -m pip install -r ./tools/requirements-python.txt
```
* Depending on your python version, you might need to add  --break-system-packages, or use venv.

## 4. Update submodules & build toolchain

```bash
git submodule update --init --recursive
cd tools
make
cd ..
```

for systems with newer CMake, you may need to change `make` to the following:
`CMAKE_POLICY_VERSION_MINIMUM=3.5 gmake`

for the n64 tools build

## 5. Prepare a base ROM

Copy your ROM to the root of this new project directory, and rename the file of the baserom to reflect the version of ROM you are using. ex: `baserom.us.rev1.z64`
* Make sure the ROM is the US version, revision 1.1 (REV A) with the sha1sum of `09f0d105f476b00efa5303a3ebc42e60a7753b7a`.

## 6. Make and Build the ROM

To start the extraction/build process, run the following command:

```bash
make -f Makefile.dc init
```

After, you can generate the type of file you desire:

#### CDI file for ODE (GDEMU, MODE, USB-GDROM, etc.)
```bash
make -f Makefile.dc cdi-ode
```

#### CDI file for CD-R (padded for performance)
```bash
make -f Makefile.dc cdi-cdr
```

#### DSISO for DreamShell
```bash
make -f Makefile.dc dsiso
```

#### Plain files for dcload-serial or dcload-ip
```bash
make -f Makefile.dc files-zip
```

#### All of the above
If you want to go wild and build all of the above types, you can do the following:
```bash
make -f Makefile.dc all
```

Good luck.
