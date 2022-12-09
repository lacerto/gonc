# Changelog

## [1.4] - 2022-12-09

### Added

* Switch `-n` starts a dry run. No files will be copied or deleted.
* Using switch `-d` files at destination that are *not* present in the source directory will be deleted.

### Changed

* Files larger than 8 MB can also be copied.

## [1.2] - 2022-11-14

### Changed

* Give better feedback to the user about which files are updated or created.

## [1.1] - 2022-11-13

### Added

* Do not copy dotfiles.

## [1.0] - 2022-11-12

### Added

* Copy files that are newer (modification time) or do not exist at destination.
* Copy only files under 8 MB.
* Create missing intermediate directories at destination.
