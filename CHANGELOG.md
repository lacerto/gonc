# Changelog

## [1.3] - 2022-11-27

### Added

* Using switch `-d` files at destination that are *not* present in the source directory will be deleted.

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
