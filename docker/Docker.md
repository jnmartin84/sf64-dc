This has been tested on Apple Silicon & Linux amd64

## 0. Pre-reqs

1. The correct Starfox 64 ROM
2. ~3 GB of free disk space
3. `Docker`
4. (optional) `screen` or `tmux` (the build takes awhile)

## 1. Prepare folder

1. Make a folder
1. Copy in `Dockerfile` and `build-sf64-dc.sh`
2. Copy in the correct ROM as `starfox64.z64`

You should see
```
$ ls -1
build-sf64-dc.sh
Dockerfile
starfox64.z64 
```

## 2. Build docker

This will take awhile (consider using `screen` or `tmux`). It may fail due to download issues; if so, run it again.

```
docker build --platform linux/amd64 -t sf64-dc-build-env .
```

## 3. Enter the docker and run the build script

```
docker run --rm -it -v "$(pwd)":/workspace sf64-dc-build-env
```

in the docker

```
cd /workspace
chmod +x build-sf64-dc.sh
./build-sf64-dc.sh
```

If it succeeds, you should see the 3 disc images in /workspace (and on your actual drive since it's mapped).