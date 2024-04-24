#include <sbdd.h>


int sbdd_alloc_disk(struct sbdd* device, const struct blk_mq_ops* mqops);

void sbdd_free_disk(struct sbdd* device);