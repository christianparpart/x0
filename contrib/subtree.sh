#! /bin/bash
# vim:ts=2:sw=2:et

print_help() {
  echo "usage:"
  echo "  subtree push 3RDPARTY_LIBRARY [REF]"
  echo "  subtree pull 3RDPARTY_LIBRARY [REF]"
  echo ""
  echo "example:"
  echo "  subtree pull libbase"
  exit 1
}

ACTION="${1}"
LIB="${2}"
REF="${3:-master}"

case "$ACTION" in
  pull)
    git subtree pull -P 3rdparty/${LIB} git@github.com:xzero/${LIB}.git ${REF}
    ;;
  push)
    git subtree push -P 3rdparty/${LIB} git@github.com:xzero/${LIB}.git ${REF}
    ;;
  *)
    print_help
    ;;
esac
