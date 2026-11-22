#!/bin/sh
# Runs a command in the doom3-riscos cross-build image with the repository at /work.
#   docker build --platform linux/amd64 -t doom3-riscos riscos/docker
cd "$(dirname "$0")/.."
exec docker run --rm --platform linux/amd64 -v "$PWD":/work -w /work ${D3_IMAGE:-doom3-riscos} "$@"
