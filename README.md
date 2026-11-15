<p align="center">
  <img src="assets/sshpass.png" width="420" alt="sshpass icon">
</p>

# sshpass

`sshpass` supplies a password to OpenSSH and other terminal-based programs in
non-interactive scripts. This implementation uses POSIX pseudo-terminals and is
designed to build natively on Haiku without third-party libraries.

## Building on Haiku

```sh
make
make test
make install
```

The default destination is `/boot/system/non-packaged/bin/sshpass`, with its
manual page under `/boot/system/non-packaged/documentation/man/man1`. `PREFIX`,
`BINDIR`, `MANDIR`, and `DESTDIR` can be overridden when packaging.

## Usage

Prefer an environment variable, password file, or inherited descriptor over a
command-line password.

### SSH

```sh
SSHPASS='secret' sshpass -e ssh user@example.com
sshpass -f ~/config/server.password ssh user@example.com
sshpass -d 3 ssh user@example.com 3< ~/config/server.password
```

### rsync over SSH

Upload a local directory using `SSHPASS`:

```sh
SSHPASS='secret' rsync -av --rsh='sshpass -e ssh' ./photos/ user@example.com:/home/user/photos/
```

Download a remote directory using a password file:

```sh
rsync -av --rsh='sshpass -f /boot/home/config/settings/server.password ssh' \
    user@example.com:/home/user/documents/ ./documents/
```

An inherited file descriptor also works well in scripts because it avoids
placing the password in the command line or environment:

```sh
rsync -av --rsh='sshpass -d 3 ssh' ./backup/ user@example.com:/srv/backup/ \
    3< /boot/home/config/settings/server.password
```

For compatibility with the established utility, `-p`, `-f`, `-d`, `-e`, `-P`,
`-v`, `-h`, and `-V` are supported. See `man sshpass` after installation.

## Security

SSH keys are safer than stored passwords. In particular, avoid `-p` when
possible because command-line arguments may be observable by other processes.

# License

This project is licensed under the MIT License.
