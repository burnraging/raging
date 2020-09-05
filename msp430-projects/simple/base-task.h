#ifndef BASE_TASK_H_
#define BASE_TASK_H_

#include "raging-global.h"

typedef enum
{
    ID_BASE_START,
    ID_BASE_TIMEOUT,
} base_id_t;

void entry_base_task(unsigned parm);

#endif  // BASE_TASK_H_