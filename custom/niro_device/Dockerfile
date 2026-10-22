FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake pkg-config libeigen3-dev libssl-dev libzmq3-dev libwayland-dev libgles2-mesa-dev libegl1-mesa-dev ocl-icd-opencl-dev libglib2.0-dev libpcre2-dev libdouble-conversion-dev libfontconfig1-dev libx11-dev libxext-dev libxrender-dev libxkbcommon-dev libdbus-1-dev libpng-dev libjpeg-dev zlib1g-dev && rm -rf /var/lib/apt/lists/*
ENV LD_LIBRARY_PATH=/device-sdk/usr/local/lib:/device-sdk/usr/lib/aarch64-linux-gnu:/device-sdk/lib/aarch64-linux-gnu
ENV PATH=/device-sdk/usr/lib/qt5/bin:/device-sdk/usr/local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
WORKDIR /work
