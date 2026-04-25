#!/usr/bin/env bash
set -e

git submodule init
git submodule update

TARGETS=("${@:-kickstart}")

# In prkl, "all" means "both spiffy patches" — the upstream all (esp32+u64ii)
# isn't a release artifact for us.
for i in "${!TARGETS[@]}"; do
    [ "${TARGETS[$i]}" = "all" ] && TARGETS[$i]="spiffy"
done

docker run --rm -v "$(pwd)":/__w ghcr.io/gideonz/riscv:latest make "${TARGETS[@]}"

VERSION=$(sed -n 's/^#define[[:space:]]\+APPL_VERSION_NUMBER[[:space:]]\+"\([^"]*\)".*/\1/p' software/application/versions.h)

for t in "${TARGETS[@]}"; do
    case "$t" in
        kickstart|spiffy)
            [ -f kickstart.ue2 ] && mv -f kickstart.ue2 "Prkl_SoftPatch_${VERSION}.ue2"
            ;;
    esac
    case "$t" in
        kickburn|spiffy)
            [ -f kickburn.ue2 ] && mv -f kickburn.ue2 "Prkl_FlashPatch_${VERSION}.ue2"
            ;;
    esac
done

echo "Built version: $VERSION"
ls -la Prkl_*.ue2 2>/dev/null || true
