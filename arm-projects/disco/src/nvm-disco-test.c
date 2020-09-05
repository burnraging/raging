
#include "raging-global.h"

#include "nvm-tag.h"
#include "nvm-desc.h"
#include "nvm-stm32f4xx.h"

// Start tag numbers at 1
// Tag number not to exceed 'MAX_TAGS_DATA'
typedef enum
{
    TAG1 = 1,
    TAG2 = 2,
} tag_t;

typedef struct
{
    uint8_t   a;
    uint8_t   b;
    uint8_t   c;
} tag1_data_t;


void nvm_test(void)
{
    tag1_data_t   tag1_data;
    tag1_data_t  *tag1_data_ptr;
    uint16_t      tag1_length = 0;
    int           sector_to_reclaim;

#if 0      // enable this 1 time to clear tag sectors
    UNUSED(tag1_data);
    UNUSED(tag1_data_ptr);
    UNUSED(tag1_length);
    UNUSED(sector_to_reclaim);

// Sub in when using data sectors
//    stm_flash_init(STM_FLASH_VOLTAGE_3);
//    stm_flash_erase(0, 0);
//    stm_flash_erase(0, 1);
//    stm_flash_erase(0, 2);
// Sub in when USE_REAR_SECTORS is set
//    stm_flash_erase(0, 10);
//    stm_flash_erase(0, 11);

// Substitute all of above with this:
    NVMtotalReset(SPACE_DATA);

#else
    // Selecting 'true' to recover junk sectors
    NVMinit(true);
    sector_to_reclaim = NVMgarbageCollectNoErase(SPACE_DATA,
                                                 SCORE_MOST_UNCLEAN);
    if (RFAIL_NOT_FOUND != sector_to_reclaim)
    {
        NVMeraseSectorForeground(SPACE_DATA, (uint16_t)sector_to_reclaim);
    }

    NVMreadTag(SPACE_DATA, TAG1, (void **)&tag1_data_ptr, &tag1_length);
    if ((NULL == tag1_data_ptr) || (0 == tag1_length))
    {
        tag1_data.a = 1;
        tag1_data.b = 2;
        tag1_data.c = 3;

        NVMwriteTag(SPACE_DATA, TAG1, &tag1_data, sizeof(tag1_data_t));
    }
#endif
}
