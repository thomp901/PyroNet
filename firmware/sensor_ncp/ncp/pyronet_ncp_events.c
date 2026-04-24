#include "pyronet_ncp_events.h"

#include <pthread.h>
#include <string.h>

#include "pyronet_ncp_local.h"

typedef struct pyronet_ncp_event_queue
{
    bool initialized;
    pyronet_ncp_event_t queue[PYRONET_EVENT_QUEUE_LEN];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    pthread_mutex_t lock;
} pyronet_ncp_event_queue_t;

static pyronet_ncp_event_queue_t pyronetNcpEventQueue;

static void pyronetNcpEventsReset(void)
{
    pyronetNcpEventQueue.head = 0U;
    pyronetNcpEventQueue.tail = 0U;
    pyronetNcpEventQueue.count = 0U;
    memset(pyronetNcpEventQueue.queue, 0, sizeof(pyronetNcpEventQueue.queue));
}

static void pyronetNcpEventsLock(void)
{
    (void)pthread_mutex_lock(&pyronetNcpEventQueue.lock);
}

static void pyronetNcpEventsUnlock(void)
{
    (void)pthread_mutex_unlock(&pyronetNcpEventQueue.lock);
}

static void pyronetNcpEventsPushLocked(const pyronet_ncp_event_t *event)
{
    if (pyronetNcpEventQueue.count >= PYRONET_EVENT_QUEUE_LEN)
    {
        pyronetNcpEventQueue.head = (uint8_t)((pyronetNcpEventQueue.head + 1U) % PYRONET_EVENT_QUEUE_LEN);
        pyronetNcpEventQueue.count--;
    }

    pyronetNcpEventQueue.queue[pyronetNcpEventQueue.tail] = *event;
    pyronetNcpEventQueue.tail = (uint8_t)((pyronetNcpEventQueue.tail + 1U) % PYRONET_EVENT_QUEUE_LEN);
    pyronetNcpEventQueue.count++;
}

static pyronet_ncp_event_t *pyronetNcpEventsFindLocked(uint8_t msg_type)
{
    uint8_t index;
    uint8_t slot;

    for (index = 0U; index < pyronetNcpEventQueue.count; ++index)
    {
        slot = (uint8_t)((pyronetNcpEventQueue.head + index) % PYRONET_EVENT_QUEUE_LEN);
        if (pyronetNcpEventQueue.queue[slot].msg_type == msg_type)
        {
            return &pyronetNcpEventQueue.queue[slot];
        }
    }

    return NULL;
}

static void pyronetNcpEventsPush(const pyronet_ncp_event_t *event)
{
    if (event == NULL)
    {
        return;
    }

    pyronetNcpEventsLock();
    pyronetNcpEventsPushLocked(event);
    pyronetNcpEventsUnlock();
}

bool pyronet_ncp_events_init(void)
{
    if (pyronetNcpEventQueue.initialized)
    {
        pyronetNcpEventsLock();
        pyronetNcpEventsReset();
        pyronetNcpEventsUnlock();
        return true;
    }

    memset(&pyronetNcpEventQueue, 0, sizeof(pyronetNcpEventQueue));
    if (pthread_mutex_init(&pyronetNcpEventQueue.lock, NULL) != 0)
    {
        return false;
    }

    pyronetNcpEventQueue.initialized = true;
    pyronetNcpEventsReset();
    return true;
}

bool pyronet_ncp_events_next(pyronet_ncp_event_t *out_event)
{
    if (out_event == NULL)
    {
        return false;
    }

    pyronetNcpEventsLock();
    if (pyronetNcpEventQueue.count == 0U)
    {
        pyronetNcpEventsUnlock();
        return false;
    }

    *out_event = pyronetNcpEventQueue.queue[pyronetNcpEventQueue.head];
    pyronetNcpEventQueue.head = (uint8_t)((pyronetNcpEventQueue.head + 1U) % PYRONET_EVENT_QUEUE_LEN);
    pyronetNcpEventQueue.count--;
    pyronetNcpEventsUnlock();

    return true;
}

void pyronet_ncp_events_queue_registration_needed(uint8_t reason)
{
    pyronet_ncp_event_t event;
    pyronet_ncp_event_t *existing_event;

    event.msg_type = PYRONET_HOST_MSG_REGISTRATION_NEEDED;
    event.payload_len = sizeof(event.payload.registration_needed);
    event.payload.registration_needed.reason = reason;

    pyronetNcpEventsLock();
    existing_event = pyronetNcpEventsFindLocked(PYRONET_HOST_MSG_REGISTRATION_NEEDED);
    if (existing_event != NULL)
    {
        existing_event->payload.registration_needed.reason = reason;
        pyronetNcpEventsUnlock();
        return;
    }

    pyronetNcpEventsPushLocked(&event);
    pyronetNcpEventsUnlock();
}

void pyronet_ncp_events_queue_parent_changed(uint8_t change_reason)
{
    pyronet_ncp_event_t event;

    event.msg_type = PYRONET_HOST_MSG_PARENT_CHANGED;
    event.payload_len = sizeof(event.payload.parent_changed);
    event.payload.parent_changed.change_reason = change_reason;
    pyronetNcpEventsPush(&event);
}

void pyronet_ncp_events_queue_tx_result(uint8_t request_type, uint8_t status, uint8_t detail)
{
    pyronet_ncp_event_t event;

    event.msg_type = PYRONET_HOST_MSG_TX_RESULT;
    event.payload_len = sizeof(event.payload.tx_result);
    event.payload.tx_result.request_type = request_type;
    event.payload.tx_result.status = status;
    event.payload.tx_result.detail = detail;
    event.payload.tx_result.reserved = 0U;
    pyronetNcpEventsPush(&event);
}

void pyronet_ncp_events_queue_time_sync_update(uint32_t unix_time_s)
{
    pyronet_ncp_event_t event;

    event.msg_type = PYRONET_HOST_MSG_TIME_SYNC_UPDATE;
    event.payload_len = sizeof(event.payload.time_sync_update);
    event.payload.time_sync_update.unix_time_s = unix_time_s;
    pyronetNcpEventsPush(&event);
}

void pyronet_ncp_events_queue_neighbor_alert_rx(uint16_t node_id, uint8_t risk_level, uint32_t timestamp)
{
    pyronet_ncp_event_t event;

    event.msg_type = PYRONET_HOST_MSG_NEIGHBOR_ALERT_RX;
    event.payload_len = sizeof(event.payload.neighbor_alert_rx);
    event.payload.neighbor_alert_rx.node_id = node_id;
    event.payload.neighbor_alert_rx.risk_level = risk_level;
    event.payload.neighbor_alert_rx.timestamp = timestamp;
    pyronetNcpEventsPush(&event);
}

void pyronet_ncp_events_queue_config_update_rx(const pyronet_host_config_update_received_v1_t *config)
{
    pyronet_ncp_event_t event;

    if (config == NULL)
    {
        return;
    }

    event.msg_type = PYRONET_HOST_MSG_CONFIG_UPDATE_RX;
    event.payload_len = sizeof(event.payload.config_update_rx);
    event.payload.config_update_rx = *config;
    pyronetNcpEventsPush(&event);
}
