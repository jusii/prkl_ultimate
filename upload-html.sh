#!/bin/sh

# Upload html/index.html to /Flash/html/index.html on the device.
#
# SoftPatch (kickstart) only loads ultimate.app into RAM; it doesn't
# touch the flash filesystem, so any changes to the web UI do NOT
# appear after a SoftPatch boot. This script ships the new HTML over
# FTP so the on-device web server picks it up.
#
# Persistence: survives reboots and SoftPatches. A full FlashPatch
# update will overwrite it with whatever HTML is bundled in the
# update binary.

DEFAULT_IP=c64u

# use IP from command-line, otherwise the default
IP=${1:-$DEFAULT_IP}

set -e

curl -T html/index.html ftp://$IP/Flash/html/index.html

echo "Uploaded html/index.html -> ftp://$IP/Flash/html/"
echo "Refresh your browser at http://$IP/ to see changes."
