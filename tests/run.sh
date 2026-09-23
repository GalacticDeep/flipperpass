#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_bin="$(mktemp /tmp/flipperpass-tests.XXXXXX)"
trap 'rm -f "$test_bin"' EXIT
${CC:-cc} -std=c11 -Wall -Wextra -Werror -g -fsanitize=${SANITIZERS:-undefined} \
    -I. protocol.c tests/test_protocol.c -o "$test_bin"
"$test_bin"
