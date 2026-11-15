<p align="center">
  <img src="assets/sshpass.png" width="420" alt="sshpass icon">
</p>

<pre align="center">
███████╗███████╗██╗  ██╗██████╗  █████╗ ███████╗███████╗
██╔════╝██╔════╝██║  ██║██╔══██╗██╔══██╗██╔════╝██╔════╝
███████╗███████╗███████║██████╔╝███████║███████╗███████╗
╚════██║╚════██║██╔══██║██╔═══╝ ██╔══██║╚════██║╚════██║
███████║███████║██║  ██║██║     ██║  ██║███████║███████║
╚══════╝╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝  ╚═╝╚══════╝╚══════╝
</pre>

<p align="center">
  <img alt="Platform: Haiku" src="https://img.shields.io/badge/platform-Haiku-ffcc33?style=for-the-badge">
  <img alt="Language: C" src="https://img.shields.io/badge/language-C-4b8bbe?style=for-the-badge">
  <img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-62c462?style=for-the-badge">
  <img alt="Dependencies: none" src="https://img.shields.io/badge/dependencies-none-9b59b6?style=for-the-badge">
</p>

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

### 🟢 SSH

Connect with the password written directly in the command:

```sh
sshpass -p 'secret' ssh user@example.com
```

### 🔵 rsync over SSH

Upload the local `photos` folder to the server:

```sh
sshpass -p 'secret' rsync -av ./photos/ user@example.com:/home/user/photos/
```

Download the server's `photos` folder to your computer:

```sh
sshpass -p 'secret' rsync -av user@example.com:/home/user/photos/ ./photos/
```

In these examples, replace `secret`, `user`, and `example.com` with your own
password, username, and server address. The trailing `/` means "copy the files
inside this folder."

For a safer command that does not show the password in the process list, use
the `SSHPASS` environment variable:

```sh
SSHPASS='secret' sshpass -e rsync -av user@example.com:/home/user/photos/ ./photos/
```

For compatibility with the established utility, `-p`, `-f`, `-d`, `-e`, `-P`,
`-v`, `-h`, and `-V` are supported. See `man sshpass` after installation.

## Security

SSH keys are safer than stored passwords. In particular, avoid `-p` when
possible because command-line arguments may be observable by other processes.

## License

This Haiku version was written independently from scratch and we've released it 
under the [MIT License](LICENSE). The original `sshpass` is a separate project 
released under the [GPL](https://github.com/kevinburke/sshpass/blob/master/LICENSE); we share its
handy command-line interface, but none of its source code.

