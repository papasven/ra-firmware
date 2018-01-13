
#include "lpclib.h"

#include "app.h"
#include "beacon.h"
#include "beaconprivate.h"


#define BEACON_MAX_SONDES       3


/* Points to list of instance structures */
static BEACON_InstanceData *instanceList;


/* Get a new instance data structure for a new sonde */
static BEACON_InstanceData *_BEACON_getInstanceDataStructure (float frequencyMHz)
{
    BEACON_InstanceData *p;
    BEACON_InstanceData *instance;

    /* Check if we already know a sonde on that frequency.
     * Count the number of sondes while traversing the list.
     */
    int numSondes = 0;
    p = instanceList;
    while (p) {
        if (p->rxFrequencyMHz == frequencyMHz) {
            /* Found it! */
            return p;
        }

        ++numSondes;
        p = p->next;
    }

    /* Sonde not yet in list. If we have reached the maximum number of sondes
     * that we want to track in parallel, do a garbage collection now:
     * Identify the least recently used entry and delete it.
     */
    if (numSondes >= BEACON_MAX_SONDES) {
        uint32_t oldest = (uint32_t)-1;

        p = instanceList;
        instance = instanceList;
        while (p) {
            if (p->lastUpdated < oldest) {
                oldest = p->lastUpdated;
                instance = p;
            }

            p = p->next;
        }

        /* Remove entry */
        _BEACON_deleteInstance(instance);
    }

    /* We need a new instance */
    instance = (BEACON_InstanceData *)calloc(1, sizeof(BEACON_InstanceData));

    if (instance) {
        /* Prepare structure */
        instance->id = SONDE_getNewID(sonde);
        instance->rxFrequencyMHz = frequencyMHz;

        /* Insert into list */
        p = instanceList;
        if (!p) {
            instanceList = instance;
        }
        else {
            while (p) {
                if (!p->next) {
                    p->next = instance;
                    break;
                }

                p = p->next;
            }
        }
    }

    return instance;
}


/* Process the config/calib block. */
LPCLIB_Result _BEACON_processConfigFrame (
        const BEACON_Packet *rawConfig,
        BEACON_InstanceData **instancePointer,
        float rxFrequencyHz)
{
    uint32_t data;
    LPCLIB_Result result = LPCLIB_SUCCESS;


    /* Valid pointers to take the output values are required */
    if (!instancePointer) {
        return LPCLIB_ILLEGAL_PARAMETER;
    }

    /* Allocate new calib space if new sonde! */
    BEACON_InstanceData *instance = _BEACON_getInstanceDataStructure(rxFrequencyHz / 1e6f);
    *instancePointer = instance;

    if (!instance) {
        return LPCLIB_ERROR;
    }

    if (instance) {
        /* Set time marker to be able to identify old records */
        instance->lastUpdated = os_time;
    }

    return result;
}


/* Iterate through instances */
bool _BEACON_iterateInstance (BEACON_InstanceData **instance)
{
    bool result = false;

    if (instance) {
        if (*instance == NULL) {
            if (instanceList) {
                *instance = instanceList;
                result = true;
            }
        }
        else {
            *instance = (*instance)->next;
            if (*instance) {
                result = true;
            }
        }
    }

    return result;
}



/* Remove an instance from the chain */
void _BEACON_deleteInstance (BEACON_InstanceData *instance)
{
    if ((instance == NULL) || (instanceList == NULL)) {
        /* Nothing to do */
        return;
    }

    BEACON_InstanceData **parent = &instanceList;
    BEACON_InstanceData *p = NULL;
    while (_BEACON_iterateInstance(&p)) {
        if (p == instance) {                /* Found */
            *parent = p->next;              /* Remove from chain */
            free(instance);                 /* Free allocated memory */
            break;
        }

        parent = &p->next;
    }
}

