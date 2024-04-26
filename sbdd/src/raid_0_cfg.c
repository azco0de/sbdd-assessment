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
    char* _disks_str = NULL;
    char *_symbol = NULL;

    int _stripe_size, _disks_count = 0;

    substring_t _argstr[MAX_OPT_ARGS];
	
    while ((_symbol = strsep(&cfg, ";")) != NULL) 
    {
        int _token, _intval, _ret = 1;

		if (!*_symbol)
			continue;

		_token = match_token((char *)cfg, __sbdd_raid_0_config_opts_tokens, _argstr);

		if (_token < opt_last_int) 
        {
			_ret = match_int(&_argstr[0], &_intval);
			if (_ret < 0) 
            {
				pr_err("raid_0_config:: bad option arg (not int) at '%s'\n", cfg);
				return -EINVAL;
			}

			pr_info("raid_0_config:: got int token %d val %d\n", _token, _intval);

		} 
        else if (_token > opt_last_int && _token < opt_last_str) 
        {
			pr_info("raid_0_config:: got string token %d val %s\n", _token, _argstr[0].from);
		} 
        else 
        {
			pr_info("raid_0_config:: got token %d\n", _token);
		}

        switch (_token) 
        {
        case opt_stripe:
            _stripe_size = _intval;
            break;
        case opt_disks:
            _disks_str = kstrndup(_argstr[0].from, _argstr[0].to - _argstr[0].from, GFP_KERNEL);
            if (!_disks_str)
                return -ENOMEM;
            break;
        default:
            BUG_ON(_token);
        }
    }

    if(_disks_str)
    {
        while ((_symbol = strsep(&_disks_str, ",")) != NULL)
        {
            ++_disks_count;
        }
    }

    if(_disks_count == 0)
    {
        pr_err("raid_0_config:: no disks! \n");
        kfree(_disks_str);
        return -EINVAL;
    }

    _cfg->disks = kzalloc(DISK_NAME_LEN * _disks_count, GFP_KERNEL);
    if (!_cfg->disks) 
    {
        pr_err("raid_0_config:: cannot alloc config \n");
        kfree(_disks_str);
        return -ENOMEM;
    }

    _disks_count = 0;

    while ((_symbol = strsep(&_disks_str, ",")) != NULL)
    {
        strncpy(_cfg->disks[_disks_count], _symbol, DISK_NAME_LEN);

        ++_disks_count;
    }

    _cfg->disks_count = _disks_count;
    _cfg->strip_size = _stripe_size;

    kfree(_disks_str);

    return 0;
}

void sbdd_raid_0_destroy_config(sbdd_raid_0_config_t* cfg)
{
    if(cfg->disks)
    {
        kfree(cfg->disks);

        cfg->disks_count = 0;
        cfg->strip_size = 0;
    }
}