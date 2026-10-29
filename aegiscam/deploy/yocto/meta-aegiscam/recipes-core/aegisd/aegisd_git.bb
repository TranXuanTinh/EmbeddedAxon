SUMMARY = "AegisCam real-time evidence capture and chain-of-custody daemon"
DESCRIPTION = "Mission-critical C++17 device daemon managing pre-event ring buffering, hash-chained segment writing, and A/B OTA state."
HOMEPAGE = "https://github.com/tinhtran/aegiscam"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

SRC_URI = "git://github.com/tinhtran/aegiscam.git;protocol=https;branch=main"
SRCREV = "${AUTOREV}"
PV = "1.0+git${SRCPV}"

S = "${WORKDIR}/git"

inherit cmake systemd useradd

EXTRA_OECMAKE = " \
    -DAEGIS_BUILD_TESTS=OFF \
    -DAEGIS_ENABLE_FUZZ=OFF \
    -DCMAKE_BUILD_TYPE=Release \
"

USERADD_PACKAGES = "${PN}"
GROUPADD_PARAM:${PN} = "-r aegis"
USERADD_PARAM:${PN} = "-r -g aegis -G video -d /var/lib/aegis -s /sbin/nologin aegis"

SYSTEMD_SERVICE:${PN} = "aegisd.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${S}/deploy/systemd/aegisd.service ${D}${systemd_system_unitdir}/aegisd.service
    install -d -m 0700 -o aegis -g aegis ${D}${localstatedir}/lib/aegis
}

FILES:${PN} += "${systemd_system_unitdir}/aegisd.service ${localstatedir}/lib/aegis"
