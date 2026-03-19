#!/bin/bash
# setup_ssh.sh — Run on Ubuntu laptop to allow SSH from Windows PC
# Usage: bash scripts/setup_ssh.sh

mkdir -p ~/.ssh
echo "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIIX5UK2qhzR+NsNDASqWImsyDHHlrdRE0ta6fpHosbNB andao@DESKTOP-0TAVV7M" >> ~/.ssh/authorized_keys
chmod 700 ~/.ssh
chmod 600 ~/.ssh/authorized_keys
sort -u -o ~/.ssh/authorized_keys ~/.ssh/authorized_keys
echo "DONE — Windows PC SSH key added"
