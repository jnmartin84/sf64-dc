# Star Fox 64 for Sega Dreamcast (2025/12/05)

This is a Star Fox 64 port for the Sega Dreamcast, based on [sonicdcer](https://github.com/sonicdcer/)'s excellent [Star Fox 64 decompilation](https://github.com/sonicdcer/sf64) (that also powers [Starship](https://github.com/HarbourMasters/Starship)).

![Corneria](/media/screenshot1.png)
![Sector Y](/media/screenshot2.png)
![Venom](/media/screenshot3.png)

Check out [footage of the Dreamcast version](https://www.youtube.com/playlist?list=PLXx8QBt8z_x8jq1VrSsZ8wAi76keeBIkO) running on real hardware.

**A direct CDI download of Star Fox 64 for Dreamcast is not provided, nor will it be provided.**

You must build it yourself from your own N64 ROM. There is no exception to this. Do not ask.

Running Star Fox 64 for Dreamcast on a Dreamcast emulator is **not** supported. It may or may not work correctly. This port is intended to be used only on real hardware. Just emulate an N64 and move on if that's your plan.

**Rumble**:
Force-feedback is supported and tested with the following devices:
- Sega Jump / Puru Pack
- Performance TremorPak
- Nyko DC Hyper Pak
- Retro Fighters Striker DC Wireless built-in

**Saving**: 
If a VMU is present, the cartridge EEPROM data will be saved. 3 blocks are required.

**Dreamcast controls**:
- A: Fire laser
- X: Fire bomb
- Y: Boost
- B: Brake
- Down + Y: Somersault
- Down + B: U-Turn
- L Trigger: Tilt left 90 degrees
- R Trigger: Tilt right 90 degrees
- D-Pad Up: Change camera
- D-Pad Right: Answer ROB call

The training mode and in-game messages have been updated with the new button mappings.

## 1. Setup Dreamcast tooling
Set up a KallistiOS environment using KallistiOS `v2.2.1` with a GCC `14.x` toolchain.
If you don't know how to do this, check the [Getting Started with Dreamcast Development](https://dreamcast.wiki/Getting_Started_with_Dreamcast_development) guide.
1. At the `Configuring the dc-chain script` step, make sure you use the `14.3.0` toolchain profile, **not** the default `stable` toolchain.
2. At the `Setting up the environment settings` step, alter your `environ.sh` file:
   - Enable O3 optimizations by changing
     - from `export KOS_CFLAGS="${KOS_CFLAGS} -O2"`
     - to `export KOS_CFLAGS="${KOS_CFLAGS} -O3"`
   - Enable fat LTO by changing
     - from `#export KOS_CFLAGS="${KOS_CFLAGS} -freorder-blocks-algorithm=simple -flto=auto"`
     - to `export KOS_CFLAGS="${KOS_CFLAGS} -flto=auto -ffat-lto-objects"`
     - (note the removal of the `#` at the beginning of the line and the removal of `-freorder-blocks-algorithm=simple` from the middle of it)

After applying those changes to `environ.sh`, run `source /opt/toolchains/dc/kos/environ.sh` to apply the new settings to your environment and compile KallistiOS and the `libGL` KOS port as usual via the instructions.

Using any other version of KallistiOS or the toolchain is unsupported and may not work.

## 2. Clone and enter the repository

```bash
git clone https://github.com/jnmartin84/sf64-dc.git
cd sf64-dc
```

## 3. Install python dependencies

The build process has a few python packages required that are located in `/tools/requirements-python.txt`.

To install them simply run in a terminal:

```bash
python3 -m pip install -r ./tools/requirements-python.txt
```

If you get an error about an externally managed environment, you can create a local virtual environment for this project:
```bash
python3 -m venv .
. ./bin/activate
python3 -m pip install -r ./tools/requirements-python.txt
```

## 4. Update submodules & build N64 tools

```bash
git submodule update --init --recursive
cd tools
make
cd ..
```

If you are on macOS, you should use `gmake` instead of `make` for this step. You may need to install `gmake` with `brew`.

If you are on a system with a newer CMake and get this error:
```
Compatibility with CMake < 3.5 has been removed from CMake.
```
You may need to change the `make` command above to the following:

`CMAKE_POLICY_VERSION_MINIMUM=3.5 make`

## 5. Prepare the Star Fox 64 ROM file

You need the US version, revision 1.1 (REV A) ROM file for Star Fox 64.
The correct sha1sum is `09f0d105f476b00efa5303a3ebc42e60a7753b7a`

Copy your ROM to the root of this project directory, and rename the file to `baserom.us.rev1.z64`.

## 6. Process the ROM and build assets

Process the ROM file by running the following command:

```bash
make init
```
## 7. Build the Dreamcast version
When the above command completes, you can generate the type of file for Dreamcast you desire:

### CDI files
Creating CDI files requires [mkdcdisc](https://gitlab.com/simulant/mkdcdisc) to be installed and in your PATH (it's recommended to place `mkdcdisc` in `/opt/toolchains/dc/bin` to accomplish this).

#### CDI file for burning to CD-R (padded for performance)
```bash
make cdi-cdr
```

#### For ODE (GDEMU, MODE, USB-GDROM, etc.)
```bash
make cdi-ode
```

### DSISO for DreamShell ISO Loader
While a target for creating a DreamShell ISO is provided, please note that you may experience crashes that are not present when running from CD-R, ODE or dcload-ip/dcload-serial.


Also note that running a DreamShell ISO from an SD card with a DC SD adapter will provide a miserable gameplay experience full of stuttering audio. The mixer requires real-time processing guarantees that can't be met with the excessive CPU usage caused by polling the serial port. If you file an issue for this, it will be deleted and you will be ignored.


Creating DreamShell ISO files requires `mkisofs` to be installed. This is usually provided by the `cdrtools` or `cdrkit` package provided by your operating system's package manager.

```bash
make dsiso
```

### Plain files for dcload-serial or dcload-ip
```bash
make files-zip
```

### All of the above
If you want to go wild and build all of the above types, you can do the following:
```bash
make all
```

Good luck.

# Acknowledgments
- sonicdreamcaster
- darc
- gyrovorbis
- stiffpeaks
- mittens
- pcercuei
- bbhoodsta
- gpftroy
- quzar
- mrneo240
- the list goes on
