#!/bin/sh
set -eu
alpha_package_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
printf '%s\n' 'DriveStation Linux — ALFA — VERSÃO DE TESTES' \
  'Compatibilidade com roboRIO físico ainda não validada.'
exec "$alpha_package_dir/bin/drivestation-linux" "$@"
