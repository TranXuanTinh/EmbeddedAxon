SUMMARY = "Minimal hardened production rootfs image for AegisCam evidence cameras"
LICENSE = "Apache-2.0"

inherit core-image extrausers

IMAGE_FEATURES += "read-only-rootfs"

IMAGE_INSTALL = " \
    packagegroup-core-boot \
    kernel-modules \
    aegisd \
    netagent \
    wireguard-tools \
    iproute2 \
    ca-certificates \
    tpm2-tools \
"

# Disable root password login in production builds
EXTRA_USERS_PARAMS = "usermod -L root;"

IMAGE_FSTYPES = "ext4.gz wic.bmap"
