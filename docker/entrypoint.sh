#!/bin/sh
set -e

if [ -z "$BOT_TOKEN" ]; then
  echo "ERROR: BOT_TOKEN is not set"
  echo "Provide it via -e BOT_TOKEN=... or env file"
  exit 1
fi

# Prepare data dirs
mkdir -p /data/logs
cd /data

# Link logs dir for app
ln -sf /data/logs /app/logs 2>/dev/null || true

echo "Starting Birthday Bot..."
exec /app/birthday_bot

