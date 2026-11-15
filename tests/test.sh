#!/bin/sh
set -eu

trap 'rm -f tests/askpass tests/password' EXIT HUP INT TERM
${CC:-cc} -Wall -Wextra -o tests/askpass tests/askpass.c

output=$(./sshpass -p swordfish tests/askpass)
printf '%s\n' "$output" | grep -q authenticated

SSHPASS=swordfish ./sshpass -e -P 'Password:' tests/askpass >/dev/null
printf '%s\n' swordfish > tests/password
./sshpass -f tests/password -P 'Password:' tests/askpass >/dev/null
./sshpass -p swordfish -P 'Secret phrase:' tests/askpass 'Secret phrase:' >/dev/null
exec 3< tests/password
./sshpass -d 3 -P 'Password:' tests/askpass >/dev/null
exec 3<&-

set +e
./sshpass -d nope -P 'Password:' tests/askpass >/dev/null 2>&1
result=$?
set -e
[ "$result" -eq 1 ]

set +e
./sshpass -p wrong -P 'Password:' tests/askpass >/dev/null 2>&1
result=$?
set -e
[ "$result" -eq 5 ]

set +e
./sshpass -p swordfish -P 'Password:' tests/askpass 'The authenticity of host example is unknown' \
    >/dev/null 2>&1
result=$?
set -e
[ "$result" -eq 6 ]

set +e
env -u SSHPASS ./sshpass -e -P 'Password:' tests/askpass >/dev/null 2>&1
result=$?
set -e
[ "$result" -eq 1 ]

./sshpass -p swordfish -P 'Password:' sh -c 'exit 23' >/dev/null 2>&1 || result=$?
[ "$result" -eq 23 ]

echo "all tests passed"
