# Use Debian Bookworm Slim as base image (good balance of size and compatibility)
FROM debian:bookworm-slim

# Set non-interactive mode to avoid prompts during apt-get install
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    gawk patch bzip2 tar make libgmp-dev libmpfr-dev libmpc-dev gettext wget \
    libelf-dev texinfo bison flex sed git build-essential diffutils curl \
    libjpeg-dev libpng-dev python3 pkg-config cmake libisofs-dev meson ninja-build \
    rake cmake python3-pip python3-dev zip mkisofs && \
    rm -rf /var/lib/apt/lists/*

RUN [ ! -d mkdcdisc ] && git clone -q https://gitlab.com/simulant/mkdcdisc.git
RUN (cd mkdcdisc && meson setup build && meson compile -C build && meson install -C build)

# Create workspace directory
RUN mkdir -p /workspace

# Set up toolchain directory
RUN mkdir -p /opt/toolchains/dc && \
    chmod -R 755 /opt/toolchains/dc

# Clone KallistiOS
RUN git clone https://github.com/KallistiOS/KallistiOS.git -b v2.2.1 /opt/toolchains/dc/kos

# Build dc-chain toolchain
WORKDIR /opt/toolchains/dc/kos/utils/dc-chain
RUN cp Makefile.default.cfg Makefile.cfg && \
    sed -i '/^toolchain_profile=stable$/c\toolchain_profile=14.3.0' Makefile.cfg && \
    make && \
    make clean distclean

# Set up environment variables via environ.sh
WORKDIR /opt/toolchains/dc/kos
RUN cp doc/environ.sh.sample environ.sh && \
    sed -i 's/export KOS_CFLAGS="${KOS_CFLAGS} -O2"/export KOS_CFLAGS="${KOS_CFLAGS} -O3"/' environ.sh && \
    sed -i 's/#export KOS_CFLAGS="${KOS_CFLAGS} -freorder-blocks-algorithm=simple -flto=auto"/export KOS_CFLAGS="${KOS_CFLAGS} -flto=auto -ffat-lto-objects"/' environ.sh

####

# Build KallistiOS
RUN . /opt/toolchains/dc/kos/environ.sh && make

# Clone kos-ports and build libGL
RUN git clone --recursive https://github.com/KallistiOS/kos-ports /opt/toolchains/dc/kos-ports
RUN sed -i 's/-DCMAKE_BUILD_TYPE=Release/-DCMAKE_BUILD_TYPE=Release -DBUILD_SAMPLES=OFF -DBUILD_TESTS=OFF/' /opt/toolchains/dc/kos-ports/libGL/Makefile
RUN . /opt/toolchains/dc/kos/environ.sh && make -C /opt/toolchains/dc/kos-ports/libGL install clean

# Setup sf64-dc project
WORKDIR /
RUN [ ! -d "sf64-dc" ] && git clone --recursive https://github.com/jnmartin84/sf64-dc.git || true
RUN cd sf64-dc && \
    python3 -m pip install --break-system-packages -r tools/requirements-python.txt && \
    make -C tools

CMD ["/bin/bash"]
