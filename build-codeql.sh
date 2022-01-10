#!/usr/bin/env bash

export PKG_CONFIG_PATH="/usr/local/opt/ncurses/lib/pkgconfig"

pushd build-codeql

../configure \
    --target-list=x86_64-softmmu \
    --cc=clang-13 \
    --cxx=clang++ \
    --disable-cocoa \
    --disable-bsd-user \
    --disable-docs \
    --disable-gtk \
    --disable-guest-agent \
    --disable-guest-agent-msi \
    --disable-linux-user \
    --disable-sdl \
    --disable-strip \
    --disable-user \
    --disable-vde \
    --disable-werror \
    --enable-debug \
    --enable-hvf \
    --enable-sanitizers \
    --enable-tools \
    --enable-gnutls \
    --enable-nettle \
    --enable-curl \
    --enable-vnc \
    --enable-vnc-sasl \
    --enable-vnc-jpeg \
    --enable-vnc-png


codeql database create /Users/goose/work/QLDatabases/qemu.db --language=cpp --command="make" -j0

popd
