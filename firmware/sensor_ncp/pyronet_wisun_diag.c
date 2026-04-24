#include "pyronet_wisun_diag.h"

#include <string.h>

static volatile uint32_t pyronetWisunDiagSequence;
static volatile uint16_t pyronetWisunDiagEvent;
static volatile int32_t pyronetWisunDiagA;
static volatile int32_t pyronetWisunDiagB;
static volatile int32_t pyronetWisunDiagC;

void pyronet_wisun_diag_record(uint16_t event, int32_t a, int32_t b, int32_t c)
{
    pyronetWisunDiagA = a;
    pyronetWisunDiagB = b;
    pyronetWisunDiagC = c;
    pyronetWisunDiagEvent = event;
    pyronetWisunDiagSequence++;
}

bool pyronet_wisun_diag_snapshot(pyronet_wisun_diag_snapshot_t *out)
{
    uint32_t before;
    uint32_t after;

    if (out == NULL)
    {
        return false;
    }

    memset(out, 0, sizeof(*out));

    do
    {
        before = pyronetWisunDiagSequence;
        out->event = pyronetWisunDiagEvent;
        out->a = pyronetWisunDiagA;
        out->b = pyronetWisunDiagB;
        out->c = pyronetWisunDiagC;
        after = pyronetWisunDiagSequence;
    } while (before != after);

    out->sequence = after;
    return (after != 0U);
}
