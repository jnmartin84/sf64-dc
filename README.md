#### 0. Setup Dreamcast tooling

sh4-elf toolchain, KOS, kos-ports/libGL all need built and installed.

Use the `environ.sh` from `mk64-dc` for reference.

`source /opt/toolchains/dc/kos/environ.sh` 

and then...

#### 1. Install build dependencies

### Windows

For Windows 10, install WSL and a distribution by following this
[Windows Subsystem for Linux Installation Guide](https://docs.microsoft.com/en-us/windows/wsl/install-win10).
We recommend using Debian or Ubuntu 22.04 Linux distributions.

### Linux (Native or under WSL / VM)

The build process has the following package requirements:

* make
* git
* build-essential
* binutils-mips-linux-gnu
* python3
* pip3
* libpng-dev

Under Debian / Ubuntu (which we recommend using), you can install them with the following commands:

```bash
sudo apt update
sudo apt install make cmake git build-essential binutils-mips-linux-gnu python3 python3-pip clang-format-14 clang-tidy clang-tools
```

### MacOS

Install [Homebrew](https://brew.sh) and the following dependencies:
```
brew update
brew install coreutils make pkg-config tehzz/n64-dev/mips64-elf-binutils
```
#### 2. go into the repo

```bash
cd sf64
```

#### 3. Install python dependencies

The build process has a few python packages required that are located in `/tools/requirements-python.txt`.

To install them simply run in a terminal:

```bash
python3 -m pip install -r ./tools/requirements-python.txt
```
* Depending on your python version, you might need to add  --break-system-packages, or use venv.

#### 4. Update submodules & build toolchain

```bash
git submodule update --init --recursive
make -f Makefile.dc toolchain
```

#### 5. Prepare a base ROM

Copy your ROM to the root of this new project directory, and rename the file of the baserom to reflect the version of ROM you are using. ex: `baserom.us.rev1.z64`
* Make sure the ROM is the US version, revision 1.1 (REV A).

#### 6. Make and Build the ROM

To start the extraction/build process, run the following command:

```bash
make -f Makefile.dc init
```
This will create the build folders, a new folder with the assembly as well as containing the disassembly of nearly all the files containing code.

Eventually you will see an error about failing to build something in `asm/revN/region`.

Do an `rm -rf asm`.

Run `make -f Makefile.dc` again. 

Run `./generate_sf_data.sh` to make segmented ELF files for link-time symbol resolution and for binary data file generation.

Lastly, run `./link.sh` to create an output ELF file.

There's also a target for generating a CDI:
`make -f Makefile.dc cdi`

Good luck.
