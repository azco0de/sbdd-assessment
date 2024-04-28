#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/slab.h>
#include <linux/genhd.h>
#include <linux/string.h>
#include <linux/parser.h>
#include <raid_0_cfg.h>

enum {
	opt_stripe,
    opt_last_int,
	opt_disks,
    opt_last_str,
	opt_err
};

static match_table_t __sbdd_raid_0_config_opts_tokens = {
	{opt_stripe, "stripe=%d"},
	{opt_disks, "disks=%s"},
	{opt_err, NULL}
};


int sbdd_raid_0_create_config(char* cfg, sbdd_raid_0_config_t* _cfg)
{
    char *_symbol = NULL;

    int _idx = 0;

    substring_t _argstr[MAX_OPT_ARGS];

    pr_info("raid_0_config:: cfg=%s \n", cfg);
	
    while ((_symbol = strsep(&cfg, ";")) != NULL) 
    {
        int _token, _intval, _ret = 1;

		if (!*_symbol)
			continue;

		_token = match_token((char *)_symbol, __sbdd_raid_0_config_opts_tokens, _argstr);

		if (_token < opt_last_int) 
        {
			_ret = match_int(&_argstr[0], &_intval);
			if (_ret < 0) 
            {
				pr_err("raid_0_config:: bad option arg (not int) at '%s'\n", cfg);
				return -EINVAL;
			}
		} 

        switch (_token) 
        {
        case opt_stripe:
            _cfg->strip_size = _intval;
            break;
        case opt_disks:
            _cfg->disks_str = kstrndup(_argstr[0].from, _argstr[0].to - _argstr[0].from, GFP_KERNEL);
            if (!_cfg->disks_str)
                return -ENOMEM;
            break;
        default:
            BUG_ON(_token);
        }
    }

    if(_cfg->disks_str)
    {
        for(_idx = 0; _cfg->disks_str[_idx]; ++_idx)
        {
    	    if(_cfg->disks_str[_idx] == ',')
    	    {
                ++_cfg->disks_count;
            }
        }
    }

    if(_cfg->strip_size == 0)
    {
        pr_err("raid_0_config:: zero strip size! \n");
        return -EINVAL;
    }

    if(_cfg->disks_count == 0)
    {
        pr_err("raid_0_config:: no disks! \n");
        return -EINVAL;
    }

    if(++_cfg->disks_count > SDBB_RAID_0_MAX_DISKS_COUNT)
    {
        pr_err("raid_0_config:: exceeded max disks count: %d, max: %d \n",_cfg->disks_count, SDBB_RAID_0_MAX_DISKS_COUNT);
        return -EINVAL;
    }

    _idx = 0;
    while ((_symbol = strsep(&_cfg->disks_str, ",")) != NULL)
    {
        pr_info("raid_0_config:: add disk '%s' \n", _symbol);

        _cfg->disks[_idx] = _symbol;

        ++_idx;
    }

    pr_info("raid_0_config:: disks count: %d, stripe size: %d \n", _cfg->disks_count, _cfg->strip_size);

    return 0;
}

void sbdd_raid_0_destroy_config(sbdd_raid_0_config_t* cfg)
{
    if(cfg->disks_str)
        kfree(cfg->disks_str);
        
    cfg->disks_str = NULL;
    cfg->disks_count = 0;
    cfg->strip_size = 0;
}