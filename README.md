# GONC

![action badge](https://github.com/lacerto/gonc/actions/workflows/compile.yml/badge.svg)

`gonc` is a small tool written originally to synchronize gopher log (phlog) files and directories.

It simply copies files from the source directory that are either outdated or not present in the destination directory. It can also delete those files at the destination that have been deleted at source.

## Why?

My phlog is hosted on a system where I do not have access to `rsync`. The phlog is under version control, so I needed to copy the contents of the working directory to the directory that is served by the gopher server. Without `rsync` and `gonc` I just used `cp -a`.

## Compile

`gonc` is a single `C` file, so `make` can be used even without a makefile to compile it:

```sh
make gonc
```

To explicitly specify the compiler, just set `CC`:

```sh
CC=gcc make gonc
```

`gonc` depends on `fts` that is not included in `musl`. See the `Dockerfile` for how to compile using `clang` and `musl`.

## Use

```
Usage:
        gonc [-d] [-n] source_dir destination_dir
        gonc -h
        gonc -v

Options:
        -d      Delete files at destination that are not in source.
        -n      Dry run. No files will be copied or deleted.
        -h      Show this help.
        -v      Show the version number.
```