F_CRYSTAL      = 8000000L
F_CPU          = 168000000L
SYSCLOCK       = SYSCLK_FREQ_168MHz
FAMILY         = f4xx

APPLICATION_LARGE = TRUE
BOOTLOADER        = clouds_bootloader
TARGET            = achchid

PACKAGES = achchid clouds/drivers braids braids/vocalist stmlib/dsp stmlib/utils stmlib/system \
  stmlib/third_party/STM/CMSIS/DSP_Lib/CommonTables \
  stmlib/third_party/STM/CMSIS/DSP_Lib/TransformFunctions
RESOURCES = braids/resources
EXCLUDE_CC_FILES = braids.cc settings.cc ui.cc test_achchid.cc
TOOLCHAIN_PATH ?= /usr/local/arm-4.8.3/

include stmlib/makefile.inc

wav: $(TARGET_BIN)
	python -m stm_audio_bootloader.qpsk.encoder -t stm32f4 -s 48000 -b 12000 -c 6000 -p 256 $(TARGET_BIN)
