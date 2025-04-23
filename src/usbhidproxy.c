#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pico/multicore.h>
#include <pico/mutex.h>
#include <pico/sem.h>

#include <bsp/board_api.h>
#include <tusb.h>
#include "tusb_config.h"

#include <pio_usb_configuration.h>

#include "debug_func.h"


#define ARRAY_NUM(x)  (sizeof(x) / sizeof((x)[0]))


// interval_override
#include "interval_override.h"
volatile uint8_t interval_override = 0;



// static globals

static mutex_t sMutex;

// Some buffers must exist until transfer has completed.
// No callback with the key to tell which descriptor transfer has ended.
// Use return buffer and update with mutex to avoid complex control.

#define cDescriptorBufSize  256
typedef uint8_t DescriptorBuf[cDescriptorBufSize];
static volatile DescriptorBuf sDescriptorBuf;
static volatile DescriptorBuf sDescriptorReturnBuf;
static uint16_t sDescriptorStringLang = 0x0000;

static volatile size_t sMountedInstanceNum = 0;
static volatile size_t sInstanceNum = 0;
static volatile bool sIsDeviceThere = false;
static volatile bool sIsAllInstanceMounted = false;
static volatile bool sIsInstanceMountedArray[HID_INSTANCE_MAX];


static volatile uint8_t sDeviceAddrArray[HID_INSTANCE_MAX];



// Only one configuration is supported because of memory constraint.
#define cConfigurationBufSize  256
typedef uint8_t ConfigurationBuf[cConfigurationBufSize];
static volatile ConfigurationBuf sConfigurationBuf;
static volatile ConfigurationBuf sConfigurationReturnBuf;


// Note that only one language is supported. (memory constraint)
#define cStringBufSize  256
// Possible strings are lang, manufacturer, product, serial number and HID instances.
// 0xEE is not supported.
#define cDescriptorStringMax  (4 + HID_INSTANCE_MAX)
typedef uint8_t StringBuf[cStringBufSize];
static volatile StringBuf sStringBufArray[cDescriptorStringMax];
static volatile StringBuf sStringReturnBufArray[cDescriptorStringMax];

static uint8_t sStringIndexArray[cDescriptorStringMax - 1];
static uint8_t sStringIndexNum = 0;

// #define cDescriptorReportBufSize  0x10000
#define cDescriptorReportBufSize  0x1000 // Memory constraint
typedef uint8_t DescriptorReportBuf[cDescriptorReportBufSize];
static volatile DescriptorReportBuf sDescriptorReportBufArray[HID_INSTANCE_MAX];


// Multi buffered
// 65536(16-bit) buffer size is required to fulfill max length,
// but RP2040 RAM is limited.
// Most mice and keyboards report several bytes.
#define cHidReportBufSize  0x100
#define cHidReportBufArrayNum  8
typedef uint8_t HidReportBuf[cHidReportBufSize];
typedef HidReportBuf HidReportBufArray[cHidReportBufArrayNum];
static volatile HidReportBufArray sHidReportBufAA[HID_INSTANCE_MAX]; // Array of Array
typedef uint16_t HidReportLengthArray[cHidReportBufArrayNum];
static volatile HidReportLengthArray sHidReportLengthAA[HID_INSTANCE_MAX];

static uint8_t sHidReportWriteIndexArray[HID_INSTANCE_MAX];
static uint8_t sHidReportReadIndexArray[HID_INSTANCE_MAX];

static semaphore_t sHidReportWriteSemArray[HID_INSTANCE_MAX];
static semaphore_t sHidReportReadSemArray[HID_INSTANCE_MAX];


enum {
    DEVICE_NONE,
    DEVICE_MOUSE,
    DEVICE_KEYBOARD,
};
static volatile uint8_t sDeviceTypeArray[HID_INSTANCE_MAX];



// Prototypes

static void core1Main(void);

static void hidTask(void);

static void initData(void);


// Inline functions

inline static void savePower(void)
{
    absolute_time_t t = make_timeout_time_us(100);
    (void)best_effort_wfe_or_timeout(t);

    return;
}


int main(void)
{
    board_init();

    {
        int r = debugInit();
        if (r < 0) {
            return -1;
        }
    }

    mutex_init(&sMutex);

    initData();

    multicore_reset_core1();
    multicore_launch_core1(core1Main);

    {
        bool isAllInstanceMounted = false;
        do {
            mutex_enter_blocking(&sMutex);
            isAllInstanceMounted = sIsAllInstanceMounted;
            mutex_exit(&sMutex);
            if (isAllInstanceMounted == true) {
                break;
            }
        } while (1);
    }

    tud_init(BOARD_TUD_RHPORT);

    if (board_init_after_tusb) {
        board_init_after_tusb();
    }

    while (1) {
        tud_task(); // tinyusb device task

        hidTask();

        savePower();
    }

    return 0;
}       


static void initData(void)
{
    for (size_t i = 0; i < ARRAY_NUM(sHidReportWriteSemArray); ++i) {
        sem_init(&sHidReportWriteSemArray[i], cHidReportBufArrayNum, cHidReportBufArrayNum);
    }
    for (size_t i = 0; i < ARRAY_NUM(sHidReportReadSemArray); ++i) {
        sem_init(&sHidReportReadSemArray[i], 0, cHidReportBufArrayNum);
    }

    for (size_t i = 0; i < ARRAY_NUM(sDeviceAddrArray); ++i) {
        sDeviceAddrArray[i] = 0x00;
    }
    for (size_t i = 0; i < ARRAY_NUM(sDeviceTypeArray); ++i) {
        sDeviceTypeArray[i] = DEVICE_NONE;
    }
    for (size_t i = 0; i < ARRAY_NUM(sIsInstanceMountedArray); ++i) {
        sIsInstanceMountedArray[i] = false;
    }

    sDescriptorStringLang = 0x0000;

    for (size_t i = 0; i < ARRAY_NUM(sHidReportWriteIndexArray); ++i) {
        sHidReportWriteIndexArray[i] = 0;
    }
    for (size_t i = 0; i < ARRAY_NUM(sHidReportReadIndexArray); ++i) {
        sHidReportReadIndexArray[i] = 0;
    }

    sMountedInstanceNum = 0;
    sInstanceNum = 0;

    sIsDeviceThere = false;
    sIsAllInstanceMounted = false;

    return;
}


static void core1Main(void)
{
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    tuh_init(BOARD_TUH_RHPORT);

    while (true) {
        tuh_task();

        savePower();
    }

    return;
}


static void vCopy(volatile void *restrict dst,
                  volatile const void *restrict src,
                  size_t n)
{
    volatile uint8_t *d = dst;
    const volatile uint8_t *s = src;

    for (size_t i = 0; i < n; ++i) {
        *d++ = *s++;
    }

    return;
}


static void vZero(volatile void *restrict dst, size_t n)
{
    volatile uint8_t *d = dst;
    for (size_t i = 0; i < n; ++i) {
        *d++ = 0;
    }

    return;
}


static void hostReport(uint8_t dAddr, uint8_t instance)
{
    bool r;

    do {
        r = tuh_hid_receive_report(dAddr, instance);
        if (r == true) {
            break;
        }
        debugPrintf("Failed to tuh_hid_receive_report().");
        sleep_ms(1);
    } while (1);

    return;
}


void tuh_hid_mount_cb(uint8_t deviceAddr, uint8_t instance,
                      uint8_t const *descriptorReport, uint16_t descriptorLength)
{
#if 0
    debugPrintf("mount deviceAddr = %02x, %u, %08x, %u",
                 deviceAddr, (uint32_t)instance, (uint32_t)descriptorReport, (uint32_t)descriptorLength);
    {
        for (size_t i = 0; i < descriptorLength; i += 8) {
            debugPrintf("%02x %02x %02x %02x %02x %02x %02x %02x",
                         descriptorReport[i],
                         descriptorReport[i + 1],
                         descriptorReport[i + 2],
                         descriptorReport[i + 3],
                         descriptorReport[i + 4],
                         descriptorReport[i + 5],
                         descriptorReport[i + 6],
                         descriptorReport[i + 7]);
        }
    }
#endif

    mutex_enter_blocking(&sMutex);

    sDeviceAddrArray[instance] = deviceAddr;

    if (sMountedInstanceNum == 0) {
        for (size_t i = 0; i < ARRAY_NUM(sStringIndexArray); ++i) {
            sStringIndexArray[i] = 0;
        }
        {
            DescriptorBuf buf;
            (void)memset(buf, 0, sizeof(buf));
            uint8_t r = tuh_descriptor_get_device_sync(deviceAddr, buf, sizeof(buf));
            if (r == XFER_RESULT_SUCCESS) {
                vCopy(sDescriptorBuf, buf, sizeof(buf));

                // Quick hack: if bMaxPacketSize0 is small, it seems cause error by inconsistency.
                sDescriptorBuf[7] = CFG_TUD_ENDPOINT0_SIZE;

                {
                    uint8_t manufacturer = sDescriptorBuf[14];
                    sStringIndexArray[sStringIndexNum++] = manufacturer;
                    uint8_t product = sDescriptorBuf[15];
                    sStringIndexArray[sStringIndexNum++] = product;
                    uint8_t sn = sDescriptorBuf[16];
                    sStringIndexArray[sStringIndexNum++] = sn;
                }
            } else {
                // TODO: assert
                mutex_exit(&sMutex);
                return; 
            }
        }
        {
            ConfigurationBuf buf;
            (void)memset(buf, 0, sizeof(buf));
            // Only one default configuration.
            uint8_t r = tuh_descriptor_get_configuration_sync(deviceAddr, 0, buf, sizeof(buf));
            if (r == XFER_RESULT_SUCCESS) {
                { // HID device + RP2040
                    uint8_t power = buf[8];
                    uint8_t newPower = power + 100 / 2;
                    if (newPower < power) {
                        newPower = UINT8_MAX;
                    }
                    buf[8] = newPower;
                }
                sInstanceNum = buf[4]; 
                vCopy(sConfigurationBuf, buf, sizeof(buf));
                {
                    for (size_t i = 0; i < sInstanceNum; ++i) {
                        const uint8_t interface = buf[9 + (9 + 9 + 7) * i + 8];
                        sStringIndexArray[sStringIndexNum++] = interface;
                    }
                }
            } else {
                // TODO: assert
                mutex_exit(&sMutex);
                return;
            }
        }
        {
            StringBuf buf;
            uint16_t lang = 0x0000;
            uint8_t r;

            (void)memset(buf, 0, sizeof(buf));
            r = tuh_descriptor_get_string_sync(deviceAddr, 0, 0, buf, sizeof(buf));
            if (r == XFER_RESULT_SUCCESS) {
                // Only one language supported.
                buf[0] = 0x04;
                (void)memset(&buf[4], 0, sizeof(buf) - 4);

                sDescriptorStringLang = lang = (buf[3] << 8) | buf[2];
                vCopy(sStringBufArray[0], buf, sizeof(sStringBufArray[0]));
            }
            {
                for (size_t i = 0; i < sStringIndexNum; ++i) {
                    (void)memset(buf, 0, sizeof(buf));
                    uint8_t index = sStringIndexArray[i];
                    r = tuh_descriptor_get_string_sync(deviceAddr, index,
                                                       lang, buf, sizeof(buf));
                    if (r == XFER_RESULT_SUCCESS) {
                        vCopy(sStringBufArray[index],
                              buf, sizeof(sStringBufArray[index]));
                    }
                }
            }
            // Getting 0xEE causes error and need reset.  Skip.
        }
    }


    {
        vZero(sDescriptorReportBufArray[instance], sizeof(sDescriptorReportBufArray[instance]));
        vCopy(sDescriptorReportBufArray[instance], descriptorReport, descriptorLength);

        // TODO: parse descriptor report
        // Normally, the position of data to detect type is like below:
        if (descriptorReport[0] == 0x05 && descriptorReport[1] == 0x01 &&
            descriptorReport[2] == 0x09 && descriptorReport[3] == 0x02) {
            sDeviceTypeArray[instance] = DEVICE_MOUSE;
        }
        if (descriptorReport[0] == 0x05 && descriptorReport[1] == 0x01 &&
            descriptorReport[2] == 0x09 && descriptorReport[3] == 0x06) {
            sDeviceTypeArray[instance] = DEVICE_KEYBOARD;
        }
    }

    sIsInstanceMountedArray[instance] = true;

    sIsDeviceThere = true;
    sMountedInstanceNum += 1;
    if (sMountedInstanceNum == sInstanceNum) {
        sIsAllInstanceMounted = true;
    }

    mutex_exit(&sMutex);

    hostReport(deviceAddr, instance);

    return;
}


void tuh_hid_umount_cb(uint8_t deviceAddr, uint8_t instance)
{
    (void)deviceAddr;

    mutex_enter_blocking(&sMutex);

    sDeviceAddrArray[instance] = 0x00;

    sIsInstanceMountedArray[instance] = false;

    sDeviceTypeArray[instance] = DEVICE_NONE;

    sIsAllInstanceMounted = false;
    sMountedInstanceNum -= 1;
    if (sMountedInstanceNum == 0) {
        sStringIndexNum = 0;

        sIsDeviceThere = false;
    }

    mutex_exit(&sMutex);

    return;
}


void tuh_hid_report_received_cb(uint8_t deviceAddr, uint8_t instance,
                                uint8_t const *report, uint16_t length)
{
    // debugPrintf("report received cb: %02x %02x : %08x (%u)", report[0], report[1], report, length);
    do {
        mutex_enter_blocking(&sMutex);

        bool isInstanceMounted = sIsInstanceMountedArray[instance];
        if (isInstanceMounted == false) {
            mutex_exit(&sMutex);
            return;
        }

        bool r = sem_try_acquire(&sHidReportWriteSemArray[instance]);
        if (r == true) {
            break;
        }
        mutex_exit(&sMutex);
        sleep_us(1);
    } while (1);

    // Avoid buffer overrun.
    if (length > cHidReportBufSize) {
        length = cHidReportBufSize;
    }

    {
        uint8_t writeIndex = sHidReportWriteIndexArray[instance];
        vCopy(sHidReportBufAA[instance][writeIndex], report, length);
        sHidReportLengthAA[instance][writeIndex] = length;
        sHidReportWriteIndexArray[instance] = (writeIndex + 1) % cHidReportBufArrayNum;
    }

    sem_release(&sHidReportReadSemArray[instance]);

    mutex_exit(&sMutex);

    hostReport(deviceAddr, instance);

    return;
}


void tud_mount_cb(void)
{
    // debugPrintf("tud_mount_cb()");

    return;
}


// Invoked when device is unmounted
void tud_umount_cb(void)
{
    // debugPrintf("tud_umount_cb()");

    return;
}


void tud_suspend_cb(bool remoteWakeupEn)
{
    (void)remoteWakeupEn;

    return;
}


void tud_resume_cb(void)
{
    return;
}


static void hidTask(void)
{
    if (tud_hid_ready() == false) {
        if (tud_ready()) {
            // debugPrintf("hid not ready, tud ready");
        }
        if (tud_suspended() == true) {
            tud_remote_wakeup();
        }
        return;
    }
    for (size_t instance = 0; instance < sInstanceNum; ++instance) {
        bool r = sem_try_acquire(&sHidReportReadSemArray[instance]);
        if (r == true) {
            mutex_enter_blocking(&sMutex);

            {
                bool isAllInstanceMounted = sIsAllInstanceMounted;
                if (isAllInstanceMounted == false) {
                    mutex_exit(&sMutex);
                    return;
                }
            }

            uint8_t readIndex = sHidReportReadIndexArray[instance];
            uint16_t length = sHidReportLengthAA[instance][readIndex];

            sHidReportReadIndexArray[instance] = (readIndex + 1) % cHidReportBufArrayNum;

            uint8_t deviceType = sDeviceTypeArray[instance];

            mutex_exit(&sMutex);

            if (deviceType == DEVICE_MOUSE) {
                // Normally, buttons is in the first byte.
                // TODO: parse descriptor report
                uint8_t buttons = sHidReportBufAA[instance][readIndex][0];
                buttons = (buttons & ~0x03) | ((buttons >> 1) & 0x1) | ((buttons & 0x1) << 1);
                sHidReportBufAA[instance][readIndex][0] = buttons;
            } else if (deviceType == DEVICE_KEYBOARD) {
                uint8_t firstByte = sHidReportBufAA[instance][readIndex][0];
                bool isControlPushed = (firstByte & 0x1);
                size_t capsIndex = 0;

                for (size_t i = 2; i < length; ++i) {
                    uint8_t c = sHidReportBufAA[instance][readIndex][i];
                    if (c == 0x39) { // caps
                        capsIndex = i;
                        break;
                    }
                }

                // The keycodes are listed by the pushed order.
                // (First pushed key is located in the first byte,
                // second pushed key is in the second byte)
                // In some case, the order is not maintained in some case, but
                // it must work in most cases...
                if (capsIndex != 0) {
                    firstByte |= 0x01;

                    if (isControlPushed == false) {
                        for (uint16_t i = capsIndex; i < length - 1; ++i) {
                            sHidReportBufAA[instance][readIndex][i] =
                                sHidReportBufAA[instance][readIndex][i + 1];
                        }
                        sHidReportBufAA[instance][readIndex][length - 1] = 0x00;
                    }
                } else if (isControlPushed == true) {
                    firstByte &= ~0x01;
                    // To emulate FIFO, it must check new and old buffer.
                    // It loads much, but in most cases no need to do so.
                    // Put caps at the end of buffer.
                    uint16_t i = 2;
                    for ( ; i < length; ++i) {
                        if (sHidReportBufAA[instance][readIndex][i] == 0x00) {
                            sHidReportBufAA[instance][readIndex][i] = 0x39;
                            break;
                        }
                    }
                    if (i == length) {
                        for (i = 0; i < length - 1; ++i) {
                            sHidReportBufAA[instance][readIndex][i] =
                                sHidReportBufAA[instance][readIndex][i + 1];
                        }
                        sHidReportBufAA[instance][readIndex][length - 1] = 0x39;
                    }
                }

                sHidReportBufAA[instance][readIndex][0] = firstByte;
            }
          
            bool isReported = tud_hid_report(instance,
                                             (uint8_t const *)(sHidReportBufAA[instance][readIndex]), length);
#if 0
            {
                for (size_t i = 0; i < length; i += 8) {
                    debugPrintf("%02x %02x %02x %02x %02x %02x %02x %02x",
                                 sHidReportBufArray[readIndex][i],
                                 sHidReportBufArray[readIndex][i + 1],
                                 sHidReportBufArray[readIndex][i + 2],
                                 sHidReportBufArray[readIndex][i + 3],
                                 sHidReportBufArray[readIndex][i + 4],
                                 sHidReportBufArray[readIndex][i + 5],
                                 sHidReportBufArray[readIndex][i + 6],
                                 sHidReportBufArray[readIndex][i + 7]);
                }
            }
#endif
            if (isReported == false) {
                debugPrintf("Failed to tud_hid_report().");
            }
        }
    }

    return;
}


void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t length)
{
    (void)report;
    (void)length;

    // debugPrintf("tud_hid_report_complete_cb()");

    mutex_enter_blocking(&sMutex);

    if (sIsInstanceMountedArray[instance] == false) {
        mutex_exit(&sMutex);
        return;
    }

    sem_release(&sHidReportWriteSemArray[instance]);
  
    mutex_exit(&sMutex);

    return;
}


void tud_hid_report_failed_cb(uint8_t instance, hid_report_type_t reportType,
                              uint8_t const *report, uint16_t xferredBytes)
{
    (void)instance;
    (void)reportType;
    (void)report;
    (void)xferredBytes;
    
    // debugPrintf("tud_hid_report_failed_cb()");

    return;
}


uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t reportId,
                               hid_report_type_t reportType, uint8_t *buffer, uint16_t requestLength)
{
    (void)instance;
    (void)reportId;
    (void)reportType;
    (void)buffer;
    (void)requestLength;

    // debugPrintf("tud_hid_get_report_cb()");

    return 0;
}


void tud_hid_set_report_cb(uint8_t instance, uint8_t reportId,
                           hid_report_type_t reportType, uint8_t const *buf, uint16_t bufsize)
{
    (void)instance;
    (void)reportId;
    (void)reportType;
    (void)buf;
    (void)bufsize;

    // debugPrintf("tud_hid_set_report_cb()");

    return;
}


uint8_t const *tud_descriptor_device_cb(void)
{
    // debugPrintf("tud_descriptor_device_cb()");
    bool isDeviceThere = false;

    mutex_enter_blocking(&sMutex);

    isDeviceThere = sIsDeviceThere;
    if (isDeviceThere == false) {
        mutex_exit(&sMutex);
        return NULL;
    }
    vCopy(sDescriptorReturnBuf, sDescriptorBuf, cDescriptorBufSize);

    mutex_exit(&sMutex);

    return (uint8_t const *)sDescriptorReturnBuf;
}


uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;

    // debugPrintf("tud_descriptor_configuration_cb()");
    bool isDeviceThere = false;

    mutex_enter_blocking(&sMutex);

    isDeviceThere = sIsDeviceThere;
    if (isDeviceThere == false) {
        mutex_exit(&sMutex);
        return NULL;
    }

    vCopy(sConfigurationReturnBuf, sConfigurationBuf, cConfigurationBufSize);

    mutex_exit(&sMutex);

    return (uint8_t const *)sConfigurationReturnBuf;
}


uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    // debugPrintf("descriptor string cb: %02x %04x", (uint32_t)index, (uint32_t)langid);
    bool isDeviceThere = false;
    uint16_t lang = 0x0000;

    mutex_enter_blocking(&sMutex);

    isDeviceThere = sIsDeviceThere;
    lang = sDescriptorStringLang;
    if (isDeviceThere == false) {
        mutex_exit(&sMutex);
        return NULL;
    }
    
    if (index != 0x00 && langid != lang) {
        debugPrintf("not?: %02x  %04x", index, langid);
        mutex_exit(&sMutex);
        return NULL;
    }

    if (index == 0xEE) { // Not supported
        mutex_exit(&sMutex);
        return NULL;
    } else {
        vCopy(sStringReturnBufArray[index],
              sStringBufArray[index],
              sizeof(sStringReturnBufArray[index]));
        mutex_exit(&sMutex);
        return (uint16_t const *)sStringReturnBufArray[index];
    }

    debugPrintf("Not reach");
    // TODO: assert
}


uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    bool isInstanceMounted;

    mutex_enter_blocking(&sMutex);

    isInstanceMounted = sIsInstanceMountedArray[instance];
  
    mutex_exit(&sMutex);

    if (isInstanceMounted == true) {
        return (uint8_t const *)sDescriptorReportBufArray[instance];
    } else {
        return NULL;
    }
}
