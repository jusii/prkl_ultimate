#!/bin/sh

# Quickstart: "build, upload, run" automation script.
# (c) SpiffyCrew

# FTP and Telnet must be enabled on your Ultimate.
# Works better when "kickstart" is built with disabled confirmation dialog (-DFAST_LAUNCH=1).

# Ultimate IP address
DEFAULT_IP=192.168.0.64

# use IP from command-line, otherwise the default
IP=${1:-$DEFAULT_IP}

# abort shell on failures
set -e

# build
make kickstart

# upload
curl -T kickstart.ue2 ftp://$IP/Temp/kickstart.ue2

# launch
# Send 2x cursor down, 2x RETURN to navigate into Temp & launch.
{ echo -n -e "ssd\r\r";sleep 1; } | telnet $IP

