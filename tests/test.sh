#!/bin/sh
set -eu

trap 'rm -f tests/askpass tests/password' EXIT HUP INT TERM
${CC:-cc} -Wall -Wextra -o tests/askpass tests/askpass.c

output=$(./sshpass -p swordfish tests/askpass)
printf '%s\n' "$output" | grep -q authenticated

SSHPASS=swordfish ./sshpass -e tests/askpass >/dev/null
printf '%s\n' swordfish > tests/password
./sshpass -f tests/password tests/askpass >/dev/null
./sshpass -p swordfish -P 'Secret phrase:' tests/askpass 'Secret phrase:' >/dev/null
exec 3< tests/password
./sshpass -d 3 tests/askpass >/dev/null
exec 3<&-

set +e
./sshpass -d nope tests/askpass >/dev/null 2>&1
result=$?
set -e
[ "$result" -eq 1 ]

set +e
./sshpass -p wrong tests/askpass >/dev/null 2>&1
result=$?
set -e
[ "$result" -eq 5 ]

set +e
./sshpass -p swordfish tests/askpass 'The authenticity of host example is unknown' \
    >/dev/null 2>&1
result=$?
set -e
[ "$result" -eq 6 ]

set +e
env -u SSHPASS ./sshpass -e tests/askpass >/dev/null 2>&1
result=$?
set -e
[ "$result" -eq 1 ]

./sshpass -p swordfish sh -c 'exit 23' >/dev/null 2>&1 || result=$?
[ "$result" -eq 23 ]

echo "all tests passed"
