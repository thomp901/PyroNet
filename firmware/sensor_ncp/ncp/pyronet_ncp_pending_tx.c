#include "pyronet_ncp_pending_tx.h"

#include <pthread.h>
#include <string.h>

typedef struct pyronet_pending_tx
{
    bool active;
    uint16_t msg_id;
    uint16_t port;
    uint8_t request_type;
    uint8_t detail;
    uint8_t destination[PYRONET_IPV6_ADDR_LEN];
} pyronet_pending_tx_t;

typedef struct pyronet_ncp_pending_tx_state
{
    bool initialized;
    pyronet_pending_tx_t slots[PYRONET_PENDING_TX_LEN];
    pthread_mutex_t lock;
} pyronet_ncp_pending_tx_state_t;

static pyronet_ncp_pending_tx_state_t pyronetNcpPendingTxState;

static void pyronetNcpPendingTxLock(void)
{
    (void)pthread_mutex_lock(&pyronetNcpPendingTxState.lock);
}

static void pyronetNcpPendingTxUnlock(void)
{
    (void)pthread_mutex_unlock(&pyronetNcpPendingTxState.lock);
}

bool pyronet_ncp_pending_tx_init(void)
{
    if (pyronetNcpPendingTxState.initialized)
    {
        pyronetNcpPendingTxLock();
        memset(pyronetNcpPendingTxState.slots, 0, sizeof(pyronetNcpPendingTxState.slots));
        pyronetNcpPendingTxUnlock();
        return true;
    }

    memset(&pyronetNcpPendingTxState, 0, sizeof(pyronetNcpPendingTxState));
    if (pthread_mutex_init(&pyronetNcpPendingTxState.lock, NULL) != 0)
    {
        return false;
    }

    pyronetNcpPendingTxState.initialized = true;
    return true;
}

bool pyronet_ncp_pending_tx_reserve(uint8_t *slot_index_out,
                                    uint16_t port,
                                    const uint8_t destination[static PYRONET_IPV6_ADDR_LEN],
                                    uint8_t request_type,
                                    uint8_t detail)
{
    uint8_t index;

    if ((slot_index_out == NULL) || (destination == NULL))
    {
        return false;
    }

    pyronetNcpPendingTxLock();
    for (index = 0; index < PYRONET_PENDING_TX_LEN; ++index)
    {
        if (!pyronetNcpPendingTxState.slots[index].active)
        {
            pyronetNcpPendingTxState.slots[index].active = true;
            pyronetNcpPendingTxState.slots[index].msg_id = 0U;
            pyronetNcpPendingTxState.slots[index].port = port;
            pyronetNcpPendingTxState.slots[index].request_type = request_type;
            pyronetNcpPendingTxState.slots[index].detail = detail;
            memcpy(pyronetNcpPendingTxState.slots[index].destination, destination, PYRONET_IPV6_ADDR_LEN);
            *slot_index_out = index;
            pyronetNcpPendingTxUnlock();
            return true;
        }
    }
    pyronetNcpPendingTxUnlock();

    return false;
}

void pyronet_ncp_pending_tx_release(uint8_t slot_index)
{
    if (slot_index >= PYRONET_PENDING_TX_LEN)
    {
        return;
    }

    pyronetNcpPendingTxLock();
    memset(&pyronetNcpPendingTxState.slots[slot_index], 0, sizeof(pyronetNcpPendingTxState.slots[slot_index]));
    pyronetNcpPendingTxUnlock();
}

bool pyronet_ncp_pending_tx_commit(uint8_t slot_index, uint16_t msg_id)
{
    bool committed = false;

    if ((slot_index >= PYRONET_PENDING_TX_LEN) || (msg_id == 0U))
    {
        return false;
    }

    pyronetNcpPendingTxLock();
    if (pyronetNcpPendingTxState.slots[slot_index].active)
    {
        pyronetNcpPendingTxState.slots[slot_index].msg_id = msg_id;
        committed = true;
    }
    pyronetNcpPendingTxUnlock();

    return committed;
}

bool pyronet_ncp_pending_tx_match_and_consume(uint16_t msg_id,
                                              const uint8_t destination[static PYRONET_IPV6_ADDR_LEN],
                                              uint16_t port,
                                              uint8_t *request_type_out,
                                              uint8_t *detail_out)
{
    uint8_t index;
    int match_index = -1;

    if ((request_type_out == NULL) || (detail_out == NULL))
    {
        return false;
    }

    pyronetNcpPendingTxLock();

    if (msg_id != 0U)
    {
        for (index = 0; index < PYRONET_PENDING_TX_LEN; ++index)
        {
            if (pyronetNcpPendingTxState.slots[index].active &&
                (pyronetNcpPendingTxState.slots[index].msg_id == msg_id))
            {
                match_index = (int)index;
                break;
            }
        }
    }

    if ((match_index < 0) && (destination != NULL))
    {
        for (index = 0; index < PYRONET_PENDING_TX_LEN; ++index)
        {
            if (pyronetNcpPendingTxState.slots[index].active &&
                (pyronetNcpPendingTxState.slots[index].port == port) &&
                (memcmp(pyronetNcpPendingTxState.slots[index].destination,
                        destination,
                        PYRONET_IPV6_ADDR_LEN) == 0))
            {
                match_index = (int)index;
                break;
            }
        }
    }

    if (match_index >= 0)
    {
        *request_type_out = pyronetNcpPendingTxState.slots[match_index].request_type;
        *detail_out = pyronetNcpPendingTxState.slots[match_index].detail;
        memset(&pyronetNcpPendingTxState.slots[match_index], 0, sizeof(pyronetNcpPendingTxState.slots[match_index]));
        pyronetNcpPendingTxUnlock();
        return true;
    }

    pyronetNcpPendingTxUnlock();
    return false;
}
