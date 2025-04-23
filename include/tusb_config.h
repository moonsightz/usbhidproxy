#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#define HID_INSTANCE_MAX  8

#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT  0
#endif

#ifndef BOARD_TUH_RHPORT
#define BOARD_TUH_RHPORT  1
#endif

#define CFG_TUSB_RHPORT0_MODE  (OPT_MODE_DEVICE | OPT_MODE_LOW_SPEED)
#define CFG_TUSB_RHPORT1_MODE  (OPT_MODE_HOST | OPT_MODE_LOW_SPEED)

// RHPort max operational speed can defined by board.mk
#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED  OPT_MODE_DEFAULT_SPEED
#endif


#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS  OPT_OS_PICO
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG  0
#endif

#define CFG_TUD_ENABLED  1

#define CFG_TUH_ENABLED  1
#define CFG_TUH_RPI_PIO_USB  1

#define CFG_TUD_MAX_SPEED  BOARD_TUD_MAX_SPEED



#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN  __attribute__ ((aligned(4)))
#endif


#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE  64
#endif

#define CFG_TUD_HID  HID_INSTANCE_MAX
#define CFG_TUD_CDC  0
#define CFG_TUD_MSC  0
#define CFG_TUD_MIDI  0
#define CFG_TUD_VENDOR  0

#define CFG_TUD_HID_EP_BUFSIZE  64


#define CFG_TUH_ENUMERATION_BUFSIZE  256

#define CFG_TUH_HUB  1
#define CFG_TUH_DEVICE_MAX  (CFG_TUH_HUB ? 4 : 1) // hub typically has 4 ports

#define CFG_TUH_HID  HID_INSTANCE_MAX
#define CFG_TUH_HID_EPIN_BUFSIZE  64
#define CFG_TUH_HID_EPOUT_BUFSIZE  64


#ifdef __cplusplus
 }
#endif

#endif /* #ifndef TUSB_CONFIG_H */
