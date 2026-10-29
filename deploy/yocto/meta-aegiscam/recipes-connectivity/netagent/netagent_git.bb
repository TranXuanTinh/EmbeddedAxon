SUMMARY = "AegisCam Go connectivity, WireGuard supervisor, and resumable upload agent"
DESCRIPTION = "Zero-external-dependency Go 1.22 daemon managing multi-uplink failover, DNS/DHCP health, WireGuard VPN supervision, and chunked uploads."
HOMEPAGE = "https://github.com/tinhtran/aegiscam"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

GO_IMPORT = "github.com/tinhtran/aegiscam/netagent"
SRC_URI = "git://github.com/tinhtran/aegiscam.git;protocol=https;branch=main"
SRCREV = "${AUTOREV}"
PV = "1.0+git${SRCPV}"

S = "${WORKDIR}/git/netagent"

inherit go systemd useradd

RDEPENDS:${PN} += "aegisd wireguard-tools"

USERADD_PACKAGES = "${PN}"
USERADD_PARAM:${PN} = "-r -g aegis -d /nonexistent -s /sbin/nologin aegis-net"

SYSTEMD_SERVICE:${PN} = "netagent.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_compile() {
    cd ${S}
    CGO_ENABLED=0 GOOS=${TARGET_GOOS} GOARCH=${TARGET_GOARCH} \
        ${GO} build -trimpath -ldflags="-s -w" -o ${B}/netagent ./cmd/netagent
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/netagent ${D}${bindir}/netagent

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/git/deploy/systemd/netagent.service ${D}${systemd_system_unitdir}/netagent.service
}

FILES:${PN} += "${bindir}/netagent ${systemd_system_unitdir}/netagent.service"
