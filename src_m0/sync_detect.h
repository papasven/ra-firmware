
#ifndef __SYNCDETECT_H
#define __SYNCDETECT_H


/********** MUST BE DEFINED SAME AS IN M4 PROJECT ***********/
#define IPC_S2M_DATA_SIZE                   1024
#define IPC_S2M_NUM_BUFFERS                 4

typedef struct {
    volatile uint8_t valid;
    uint8_t opcode;
    uint16_t param;
    uint32_t rxTime;

    uint8_t data8[IPC_S2M_DATA_SIZE];
} IPC_S2M;
/************************************************************/

typedef enum {
    SYNC_STATE_HUNT = 0,
    SYNC_STATE_DATA_RAW,
    SYNC_STATE_DATA_UART_8N1,
    SYNC_STATE_DATA_BIPHASE_S,
} SYNC_State;


typedef void (*PostProcessFunc)(volatile IPC_S2M *buffer);

typedef struct {
    int nPatterns;

    struct {
        int id;                             /* Identifies this sync pattern. Reported to M4 on match. */
        int nSyncLen;                       /* Length in bits of sync pattern (max. 128) */
        uint64_t pattern[2];                /* The pattern. p[0].LSB=received last, p[1].MSB=received first */
        int nMaxDifference;                 /* Maximum # differences to received pattern */
        int frameLengthBits;                /* Number of payload bits to read after sync detect */
        int startOffset;                    /* Start offset in buffer for storing first payload byte */
        SYNC_State dataState;               /* State to jump to when receiving data */
        bool inverted;                      /* Invert bits if set */
        PostProcessFunc postProcess;        /* Called after frame has been received */

        int nSubBlockBits;                  /* Limit sub-blocks to n bits */
        int nSubBlockBytes;                 /* Sub-block size (bytes) in data buffer */
    } conf[];
} SYNC_Config;


typedef struct _SyncDetectorContext *SYNC_Handle;


void SYNC_open (SYNC_Handle *pHandle);
void SYNC_configure(SYNC_Handle handle, const SYNC_Config *pConfig);

#endif
