FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://aegis-rt.cfg"
KERNEL_CONFIG_FRAGMENTS:append = " ${WORKDIR}/aegis-rt.cfg"
