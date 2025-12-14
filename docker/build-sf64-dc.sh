#!/bin/bash
set -e

echo "Setting up symlink..."
ln -sf /workspace/starfox64.z64 /sf64-dc/baserom.us.rev1.z64 || true

echo "Building project..."
cd /sf64-dc
. /opt/toolchains/dc/kos/environ.sh && make init all

echo "Copying outputs to workspace..."
mv sf64-ode.cdi /workspace/starfox64-ode.cdi || true
mv sf64-cdr.cdi /workspace/starfox64.cdi || true
mv sf64-ds.iso /workspace/starfox64.iso || true

echo "Build completed! Outputs are in /workspace"
