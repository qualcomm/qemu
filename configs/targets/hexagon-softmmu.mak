# Default configuration for hexagon-softmmu

TARGET_ARCH=hexagon
TARGET_SUPPORTS_MTTCG=y
TARGET_XML_FILES=hexagon-core.xml hexagon-hvx.xml hexagon-sys.xml
TARGET_LONG_BITS=32
TARGET_NEED_FDT=y
CONFIG_SEMIHOSTING=y
CONFIG_ARM_COMPATIBLE_SEMIHOSTING=y
