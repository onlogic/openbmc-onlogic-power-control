# SPDX-FileCopyrightText: (c) 2026 OnLogic, Inc. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileContributor: Justin Seely <justin.seely@onlogic.com>
# SPDX-FielContributor: Nicholas Hanna <nick.hanna@onlogic.com>  (SMBUS command set, MCU endpoint layer, MCU handler API, power control integration into DBus API, project structure)

# docker build --no-cache -t openbmc-build .
# docker run -it --rm -v "$(pwd):/home/builder/work" openbmc-build
# rm -rf builddir
# meson setup builddir
# ninja -C builddir

# Use the official GCC 14.2 image
FROM gcc:14.2

# Set to non-interactive to prevent apt from asking questions
ENV DEBIAN_FRONTEND=noninteractive

# Install all build dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    git \
    ninja-build \
    ccache \
    pkg-config \
    cmake \
    python3 \
    python3-pip \
    libsystemd-dev \
    libsystemd-shared \
    libboost-all-dev \
    python3-inflection \
    python3-mako \
    python3-yaml \
    python3-jsonschema && \
# Clean up apt cache
    rm -rf /var/lib/apt/lists/*

# Install the correct version of Meson using pip
RUN pip3 install --no-cache-dir --break-system-packages "meson>=1.3.0"

# Create a non-root user to work as
RUN useradd -m -s /bin/bash builder
USER builder

# Set the working directory inside the container
WORKDIR /home/builder/work

# Set the default command to start a shell
CMD ["/bin/bash"]