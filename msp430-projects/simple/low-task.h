#ifndef LOW_TASK_H_
#define LOW_TASK_H_

#include "raging-global.h"

typedef enum
{
    ID_LOW_TIMEOUT,
} low_id_t;

void entry_low_task(unsigned parm);

#endif  // LOW_TASK_H_