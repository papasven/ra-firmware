
#include <string.h>

#include "lpclib.h"

#include "adf7021.h"


/** Local device context. */
typedef struct ADF7021_Context {
    LPC_SPI_Type *spi;
    int ssel;
    GPIO_Pin muxoutPin;
    uint32_t txWord;
    uint32_t referenceFrequencyHz;
    uint32_t frequencyHz;
    uint32_t reg0Bits;
    uint32_t ifBandwidthSelect;
    volatile bool muxoutEvent;
} ADF7021_Context;


static ADF7021_Context _adf7021Context;



/* Write a command or data string */
LPCLIB_Result ADF7021_write (ADF7021_Handle handle,
                             ADF7021_Register reg,
                             uint32_t command)
{
    if (handle == LPCLIB_INVALID_HANDLE) {
        return LPCLIB_ILLEGAL_PARAMETER;
    }

    if (reg == ADF7021_REGISTER_4) {
        handle->ifBandwidthSelect = (command >> 30) & 3;
    }

#if LPCLIB_FAMILY == LPCLIB_FAMILY_LPC5410X
    volatile uint32_t *txdatReg = &LPC_FIFO->TXDATCTLSPI1;  //TODO...
#endif
#if LPCLIB_FAMILY == LPCLIB_FAMILY_LPC5411X
    volatile uint32_t *txdatReg = &handle->spi->FIFOWR;
#endif

    *txdatReg = 0
            | (command >> 16)               /* command[31:16] */
            | (((1u << handle->ssel) ^ 0x0F) << 16) /* Activate SLE (=0) */
            | (1u << 22)                    /* Ignore RX */
            | ((16 - 1) << 24)              /* Send 16 bits */
            ;
    *txdatReg = 0
            | (((command & ~0x0F) | reg) & 0xFFFF)  /* command[15:4] + register number */
            | (((1u << handle->ssel) ^ 0x0F) << 16) /* Activate SLE (=0) */
            | (1u << 20)                    /* End of transfer */
            | (1u << 21)                    /* End of frame */
            | (1u << 22)                    /* Ignore RX */
            | ((16 - 1) << 24)              /* Send 16 bits */
            ;
    while (!(handle->spi->STAT & (1u << 8)))        /* Wait until master becomes idle */ //TODO
        ;

    return LPCLIB_SUCCESS;
}



/* Raw 16-bit read from ADF7021 */
static LPCLIB_Result _ADF7021_read (ADF7021_Handle handle, uint32_t *data)
{
#if LPCLIB_FAMILY == LPCLIB_FAMILY_LPC5410X
    (void) handle;
    volatile uint32_t *txdatReg = &LPC_FIFO->TXDATCTLSPI1;  //TODO...
    volatile uint16_t *rxdatReg = (volatile uint16_t *)&LPC_FIFO->RXDATSPI1;     //TODO...
    volatile uint32_t *statReg = &LPC_FIFO->STATSPI1;       //TODO...
#endif
#if LPCLIB_FAMILY == LPCLIB_FAMILY_LPC5411X
    volatile uint32_t *txdatReg = &handle->spi->FIFOWR;
#endif

    *txdatReg = 0
            | (0 << 0)                      /* MOSI=0 */
            | (0x0F << 16)                  /* All SSEL inactive (SLE=1) */
            | ((9 - 1) << 24)               /* Receive 9 bits */
            ;
    *txdatReg = 0
            | (0 << 0)                      /* MOSI=0 */
            | (0x0F << 16)                  /* All SSEL inactive (SLE=1) */
            | (1u << 20)                    /* End of transfer */
            | (1u << 21)                    /* End of frame */
            | ((9 - 1) << 24)               /* Receive 9 bits */
            ;

    uint32_t rxdata;
#if LPCLIB_FAMILY == LPCLIB_FAMILY_LPC5410X
    while (((*statReg >> 16) & 0xFF) < 2)           /* Wait for 2 frames */
        ;
    rxdata = (*rxdatReg & 0x7F) << 9;
    rxdata |= (*rxdatReg & 0x1FF);
#endif
#if LPCLIB_FAMILY == LPCLIB_FAMILY_LPC5411X
    while (!(handle->spi->FIFOSTAT & (1u << 6)))    /* Wait for first RX frame */
        ;
    rxdata = (handle->spi->FIFORD & 0x7F) << 9;     /* Only last seven bits of first frame are valid */
    while (!(handle->spi->FIFOSTAT & (1u << 6)))    /* Wait for second RX frame */
        ;
    rxdata |= (handle->spi->FIFORD & 0x1FF);
#endif

    *data = rxdata;

    return LPCLIB_SUCCESS;
}


void ADF7021_handleSpiEvent (void)
{
}


/* Open access to device */
LPCLIB_Result ADF7021_open (LPC_SPI_Type *spi, int ssel, GPIO_Pin muxoutPin, ADF7021_Handle *pHandle)
{
    ADF7021_Context *handle = &_adf7021Context;

    handle->spi = spi;
    handle->ssel = ssel;
    handle->muxoutPin = muxoutPin;

    spi->CFG = 0
            | (1u << 2)                     /* Master */
            | (0u << 4)                     /* CPHA=0 */
            | (0u << 5)                     /* CPOL=0 */
            ;
    spi->DIV = 2;
    spi->DLY = 0
            | (1 << 0)                      /* PRE_DELAY */
            | (1 << 4)                      /* POST_DELAY */
            | (1 << 8)                      /* FRAME_DELAY */
            | (1 << 12)                     /* TRANSFER_DELAY */
            ;
    spi->CFG |= (1u << 0);                  /* Enable */

    *pHandle = handle;

    return LPCLIB_SUCCESS;
}



/* Close device */
LPCLIB_Result ADF7021_close (ADF7021_Handle *pHandle)
{
    if (*pHandle == LPCLIB_INVALID_HANDLE) {
        return LPCLIB_ILLEGAL_PARAMETER;
    }

    (*pHandle)->spi = NULL;

    *pHandle = LPCLIB_INVALID_HANDLE;

    return LPCLIB_SUCCESS;
}



/* Configure the device */
LPCLIB_Result ADF7021_ioctl (ADF7021_Handle handle, const ADF7021_Config *pConfig)
{
    if (handle == LPCLIB_INVALID_HANDLE) {
        return LPCLIB_ILLEGAL_PARAMETER;
    }

    while (pConfig->opcode != ADF7021_OPCODE_INVALID) {
        switch (pConfig->opcode) {
        case ADF7021_OPCODE_SET_REFERENCE:
            handle->referenceFrequencyHz = pConfig->referenceFrequencyHz;
            break;

        case ADF7021_OPCODE_INVALID:
            /* Nothing to do */
            break;
        }

        ++pConfig;                                      /* More config's follow */
    }

    return LPCLIB_SUCCESS;
}


/* Set PLL frequency */
LPCLIB_Result ADF7021_setPLL (ADF7021_Handle handle, uint32_t frequencyHz)
{
    uint32_t regval;

    if (handle == LPCLIB_INVALID_HANDLE) {
        return LPCLIB_ILLEGAL_PARAMETER;
    }

    handle->frequencyHz = frequencyHz;

    regval = (2u << 29) | (1u << 28) | (1u << 27);      /* MUXOUT=PLL_lock, UART/SPI mode, RX */
    regval |= ((((uint64_t)frequencyHz * 32768ull) / handle->referenceFrequencyHz) & 0x007FFFFF) << 4;
    handle->reg0Bits = regval & ~(7u << 29);

    ADF7021_write(handle, ADF7021_REGISTER_N, regval);
    while (GPIO_readBit(handle->muxoutPin) == 0)
        ;

    return LPCLIB_SUCCESS;
}


static const int _ADF7021_rssiGainCorrection[16] = {
    86,86,86,86,
    58,38,24,24,
    0, 0, 0, 0,
    0, 0, 0, 0,
};


/* Read RSSI information */
LPCLIB_Result ADF7021_readRSSI (ADF7021_Handle handle, int32_t *rssi)
{
    uint32_t regval;
    uint32_t rawdata = 0;

    if (handle == LPCLIB_INVALID_HANDLE) {
        return LPCLIB_ILLEGAL_PARAMETER;
    }

    if (rssi == NULL) {
        return LPCLIB_ILLEGAL_PARAMETER;
    }
    *rssi = -1740;

    regval = (1u << 8) | (1u << 6) | (0u << 4);     /* Enable readback, ADC=RSSI */

    if (ADF7021_write(handle, ADF7021_REGISTER_READBACK_SETUP, regval) == LPCLIB_SUCCESS) {
        if (_ADF7021_read(handle, &rawdata) == LPCLIB_SUCCESS) {
            *rssi = -1300 + (((int)rawdata & 0x7F) + _ADF7021_rssiGainCorrection[(rawdata >> 7) & 0x0F]) * 5;

            return LPCLIB_SUCCESS;
        }
    }

    return LPCLIB_ERROR;
}



/* Read frequency offset */
LPCLIB_Result ADF7021_readOffset (ADF7021_Handle handle, int32_t *offset)
{
    uint32_t regval;
    uint32_t rawdata = 0;

    if (handle == LPCLIB_INVALID_HANDLE) {
        return LPCLIB_ILLEGAL_PARAMETER;
    }

    if (offset == NULL) {
        return LPCLIB_ILLEGAL_PARAMETER;
    }
    *offset = 0;

    regval = (1u << 8) | (0 << 6);          /* Enable readback, AFC Word */

    if (ADF7021_write(handle, ADF7021_REGISTER_READBACK_SETUP, regval) == LPCLIB_SUCCESS) {
        if (_ADF7021_read(handle, &rawdata) == LPCLIB_SUCCESS) {
            //TODO must determine DEMOD_CLOCK
//            *offset = ((int64_t)rawdata * 203125ll) / 16384ll - 100000;
        *offset = rawdata;

            return LPCLIB_SUCCESS;
        }
    }

    return LPCLIB_ERROR;
}


static LPCLIB_Result _ADF7021_handleEvent (LPCLIB_Event event)
{
    ADF7021_Context *handle = &_adf7021Context;
    (void) event;

    handle->muxoutEvent = true;

    return LPCLIB_SUCCESS;
}


static GPIO_Config _muxoutEdgeEnable[] = {
    {.opcode = GPIO_OPCODE_CONFIGURE_PIN_INTERRUPT,
        {.pinInterrupt = {
            .pin = 0,
            .enable = LPCLIB_YES,
            .mode = GPIO_INT_RISING_EDGE,
            .interruptLine = GPIO_PIN_INT_2,
            .callback = _ADF7021_handleEvent, }}},

    GPIO_CONFIG_END
};

static GPIO_Config _muxoutEdgeDisable[] = {
    {.opcode = GPIO_OPCODE_CONFIGURE_PIN_INTERRUPT,
        {.pinInterrupt = {
            .pin = 0,
            .enable = LPCLIB_NO,
            .interruptLine = GPIO_PIN_INT_2, }}},

    GPIO_CONFIG_END
};


/* Perform IF filter calibration */
LPCLIB_Result ADF7021_calibrateIF (ADF7021_Handle handle, int mode)
{
    uint32_t regval;
    const uint32_t calibrationFrequencies[4][2] = {
        {78100, 116300},
        {79400, 116300},
        {78100, 119000},
        {78100, 119000},
    };

    if (handle == LPCLIB_INVALID_HANDLE) {
        return LPCLIB_ILLEGAL_PARAMETER;
    }

    /* Configure MUXOUT for FILTER_CAL_COMPLETE function */
    ADF7021_write(handle, ADF7021_REGISTER_N, handle->reg0Bits | (1u << 29));
    _muxoutEdgeEnable[0].pinInterrupt.pin = handle->muxoutPin;
    GPIO_ioctl(_muxoutEdgeEnable);

    /* Select fine or coarse calibration */
    regval = 0
            | ((mode ? 1 : 0) << 4)         /* Enable fine calibration */
            | ((handle->referenceFrequencyHz / (2 * calibrationFrequencies[handle->ifBandwidthSelect][0])) << 5)
            | ((handle->referenceFrequencyHz / (2 * calibrationFrequencies[handle->ifBandwidthSelect][1])) << 13)
            | (80u << 21)
            ;
    ADF7021_write(handle, ADF7021_REGISTER_6, regval);

    /* Start calibration */
    handle->muxoutEvent = false;
    regval = 0
            | (1u << 4)                     /* Do calibration */
            | ((handle->referenceFrequencyHz / 50000) << 5) /* --> 50 kHz */
            //TODO configure image rejection
            ;
    ADF7021_write(handle, ADF7021_REGISTER_5, regval);

    /* Wait for end of calibration */
    while (!handle->muxoutEvent)
        ;

    _muxoutEdgeDisable[0].pinInterrupt.pin = handle->muxoutPin;
    GPIO_ioctl(_muxoutEdgeDisable);

    regval &= ~(1u << 4);
    ADF7021_write(handle, ADF7021_REGISTER_5, regval);

    return LPCLIB_SUCCESS;
}
