#!/bin/bash

set -e


sudo mkdir -p /opt/fragarach/
sudo rm -rf /opt/fragarach/*

echo "Installing dependencies..."
apt install -y libbpf-dev libseccomp-dev libcap-dev clang bpftool cmake pkg-config linux-headers-$(uname -r) libasound2 libx11-6 libgl1 libglib2.0-0 build-essential libc6 libstdc++6 libssl3 libcurl4 zlib1g libncurses6 wget curl git vim nano htop netcat-openbsd nmap python3 gcc make debootstrap unzip

echo "Building Debian rootfs..."

sudo debootstrap stable /opt/fragarach/rootfs http://deb.debian.org/debian

echo "Downloading LibTorch..."
wget https://download.pytorch.org/libtorch/cpu/libtorch-shared-with-deps-2.10.0%2Bcpu.zip

echo "Extracting LibTorch..."
unzip libtorch-*.zip -d /opt/
rm libtorch-*.zip

echo "Copying prebuilt model..."
cp ./models/*.pt /opt/fragarach/

echo "Installation complete!"