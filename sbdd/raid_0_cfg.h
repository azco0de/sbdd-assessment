#ifndef _SBDD_RAID_0_CFG_H_
#define _SBDD_RAID_0_CFG_H_

#define SDBB_RAID_0_MAX_DISKS_COUNT 32

#include <linux/types.h>

struct sbdd_raid_0_config
{
    int strip_size;
    int disks_count;
    char* disks_str;
    char* disks[SDBB_RAID_0_MAX_DISKS_COUNT];
};
typedef struct sbdd_raid_0_config sbdd_raid_0_config_t;

int sbdd_raid_0_create_config(char* cfg, sbdd_raid_0_config_t* _cfg);
void sbdd_raid_0_destroy_config(sbdd_raid_0_config_t* cfg);

#endif