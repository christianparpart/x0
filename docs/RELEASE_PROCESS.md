## PRE RELEASE

0. implement all features / fix all bugs that are to be included in this release.

1. make sure everything compiles on the following platforms with their default toolchain:
  - Ubuntu 12.04, 32-bit (gcc, clang)
  - Ubuntu 12.04, 64-bit (gcc, clang)
  - Ubuntu 14.04, 32-bit (gcc, clang)
  - Ubuntu 14.04, 64-bit (gcc, clang)

2. git-tag release and push tag to github

## POST RELEASE

1. upload to PPA
  - trapni/x0 for latest Ubuntu
  - trapni/x0-precise for Ubuntu 12.04
2. update website to point to the newest release and changelog on front-page.
