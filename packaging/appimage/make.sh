#!/usr/bin/env bash
#
# Host entry point for the Altisidora AppImage build.
#
# Builds the container image (packaging/appimage/Dockerfile) and runs the recipe
# (packaging/appimage/build-in-container.sh) with the repository bind-mounted.
# No sudo: Docker Desktop runs as the host user here.
#
#   bash packaging/appimage/make.sh
#
# The artifact is left in dist/ and is not committed.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/../.." && pwd)"
IMAGE=altisidora-appimage

echo "==> Repository: $REPO"
echo "==> Building image $IMAGE"
docker build -t "$IMAGE" "$SCRIPT_DIR"

echo "==> Running the recipe inside the container"
docker run --rm \
    -v "$REPO":/work \
    -w /work \
    -e HOST_UID="$(id -u)" \
    -e HOST_GID="$(id -g)" \
    "$IMAGE" \
    bash packaging/appimage/build-in-container.sh

echo "==> Artifact:"
ls -1 "$REPO"/dist/*.AppImage
