#!/bin/sh
# PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
# macOS and Linux: double-click (macOS) or run ./flash-pocketos.command
cd "$(dirname "$0")" || exit 1
ENV="$HOME/.pocketos-factory"
if [ ! -x "$ENV/bin/python" ]; then
  command -v python3 >/dev/null || { echo "Python 3 is needed: https://www.python.org/downloads/"; exit 1; }
  echo "Setting up for the first time..."
  python3 -m venv "$ENV" && "$ENV/bin/python" -m pip install --quiet --disable-pip-version-check esptool==4.8.1 || { echo "Setup didn't finish."; exit 1; }
fi
exec "$ENV/bin/python" pocketos_factory.py "$@"
