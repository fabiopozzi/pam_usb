/*
 * Copyright (c) 2003-2007 Andrea Luzzardi <scox@sig11.org>
 *
 * This file is part of the pam_usb project. pam_usb is free software;
 * you can redistribute it and/or modify it under the terms of the GNU General
 * Public License version 2, as published by the Free Software Foundation.
 *
 * pam_usb is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/utsname.h>
#include <string.h>
#include <errno.h>
#include "mem.h"
#include "conf.h"
#include "xpath.h"
#include "log.h"

static void pusb_conf_options_get_from(t_pusb_options *opts,
		const char *from,
		xmlDoc *doc)
{
	pusb_xpath_get_string_from(doc, from, "option[@name='hostname']",
			opts->hostname, sizeof(opts->hostname));
	pusb_xpath_get_string_from(doc, from, "option[@name='system_pad_directory']",
			opts->system_pad_directory,
			sizeof(opts->system_pad_directory));
	pusb_xpath_get_string_from(doc, from, "option[@name='device_pad_directory']",
			opts->device_pad_directory,
			sizeof(opts->device_pad_directory));
	pusb_xpath_get_bool_from(doc, from, "option[@name='debug']",
			&(opts->debug));
	pusb_xpath_get_bool_from(doc, from, "option[@name='quiet']",
			&(opts->quiet));
	pusb_xpath_get_bool_from(doc, from, "option[@name='color_log']",
			&(opts->color_log));
	pusb_xpath_get_bool_from(doc, from, "option[@name='enable']",
			&(opts->enable));
	pusb_xpath_get_bool_from(doc, from, "option[@name='one_time_pad']",
			&(opts->one_time_pad));
	pusb_xpath_get_time_from(doc, from, "option[@name='pad_expiration']",
			&(opts->pad_expiration));
	pusb_xpath_get_time_from(doc, from, "option[@name='probe_timeout']",
			&(opts->probe_timeout));
	pusb_xpath_get_bool_from(doc, from, "option[@name='deny_remote']",
			&(opts->deny_remote));
}

static int pusb_conf_parse_options(t_pusb_options *opts,
		xmlDoc *doc,
		const char *user,
		const char *service)
{
	char				*xpath = NULL;
	size_t				xpath_size;
	int					i;
	struct s_opt_list	opt_list[] = {
		{ CONF_USER_XPATH, (char *)user },
		{ CONF_SERVICE_XPATH, (char *)service },
		{ NULL, NULL }
	};

	pusb_conf_options_get_from(opts, "//configuration/defaults/", doc);
    for (i = 0; i < opts->devnum; ++i ) {
        xpath_size = strlen(CONF_DEVICE_XPATH) + strlen(opts->devices[i].name) + 1;
		xpath = xmalloc(xpath_size);
		memset(xpath, 0x00, xpath_size);
		snprintf(xpath, xpath_size, CONF_DEVICE_XPATH, opts->devices[i].name, "");
		pusb_conf_options_get_from(opts, xpath, doc);
		xfree(xpath);
    }

	for (i = 0; opt_list[i].name != NULL; ++i)
	{
		xpath_size = strlen(opt_list[i].name) + strlen(opt_list[i].value) + 1;
		xpath = xmalloc(xpath_size);
		memset(xpath, 0x00, xpath_size);
		snprintf(xpath, xpath_size, opt_list[i].name, opt_list[i].value, "");
		pusb_conf_options_get_from(opts, xpath, doc);
		xfree(xpath);
	}
	return (1);
}

static int pusb_conf_device_get_property(t_pusb_device *dev,
		xmlDoc *doc,
		const char *property,
		char *store,
		size_t size)
{
	char		*xpath = NULL;
	size_t		xpath_len;
	int			retval;

    /* TODO: check dev->name */
	xpath_len = strlen(CONF_DEVICE_XPATH) + strlen(dev->name) + \
				strlen(property) + 1;
	xpath = xmalloc(xpath_len);
	memset(xpath, 0x00, xpath_len);
	snprintf(xpath, xpath_len, CONF_DEVICE_XPATH, dev->name,
			property);
	retval = pusb_xpath_get_string(doc, xpath, store, size);
	xfree(xpath);
	return (retval);
}

static int pusb_conf_parse_device(t_pusb_device *dev, xmlDoc *doc)
{
	pusb_conf_device_get_property(dev, doc, "vendor", dev->vendor,
				sizeof(dev->vendor));
	pusb_conf_device_get_property(dev, doc, "model", dev->model,
				sizeof(dev->model));
	if (!pusb_conf_device_get_property(dev, doc, "serial", dev->serial,
				sizeof(dev->serial)))
		return (0);
	pusb_conf_device_get_property(dev, doc, "volume_uuid",
			dev->volume_uuid,
			sizeof(dev->volume_uuid));
	return (1);
}

int pusb_conf_init(t_pusb_options *opts)
{
	struct utsname	u;

	memset(opts, 0x00, sizeof(*opts));
	if (uname(&u) == -1)
	{
		log_error("uname: %s\n", strerror(errno));
		return (0);
	}
	strncpy(opts->hostname, u.nodename, sizeof(opts->hostname) - 1);
	if (strlen(u.nodename) > sizeof(opts->hostname))
		log_info("Hostname \"%s\" is too long, truncating to \"%s\".\n",
				u.nodename, opts->hostname);
	strcpy(opts->system_pad_directory, ".pamusb");
	strcpy(opts->device_pad_directory, ".pamusb");
	opts->probe_timeout = 10;
	opts->enable = 1;
	opts->debug = 0;
	opts->quiet = 0;
	opts->color_log = 1;
	opts->one_time_pad = 1;
	opts->pad_expiration = 3600;
	opts->deny_remote = 1;
	return (1);
}

int pusb_conf_parse(const char *file, t_pusb_options *opts,
		const char *user, const char *service)
{
	xmlDoc	*doc = NULL;
	int		ndev;
	char	device_xpath[sizeof(CONF_USER_XPATH) + CONF_USER_MAXLEN + \
		sizeof("device")];

	log_debug("Parsing settings...\n",
			user, service);
	if (strlen(user) > CONF_USER_MAXLEN)
	{
		log_error("Username \"%s\" is too long (max: %d).\n", user,
				CONF_USER_MAXLEN);
		return (0);
	}
	if (!(doc = xmlReadFile(file, NULL, 0)))
	{
		log_error("Unable to parse \"%s\".\n", file);
		return (0);
	}
	snprintf(device_xpath, sizeof(device_xpath), CONF_USER_XPATH, user,
			"device");
    printf("device xpath %s\n", device_xpath);
	ndev = pusb_xpath_get_string_array(doc,
			device_xpath,
			opts->devices,
			sizeof(opts->devices[0].name));

    if(ndev > 0)
    {
        opts->devnum = ndev;
        int i, ret;
        for(i=0;i<ndev;i++) {
            printf("device %d name %s\n", i, opts->devices[i].name);
            ret = pusb_conf_parse_device(&(opts->devices[i]), doc);
            if(ret) {
                printf("device found: vendor %s\nmodel %s\nserial %s\n",
                        opts->devices[i].vendor, opts->devices[i].model,
                opts->devices[i].serial);
            }
        }
    }
    else {
        /*if (!retval || !pusb_conf_parse_device(opts, doc))*/
        log_error("No device configured for user \"%s\".\n", user);
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return (0);
    }
    if (!pusb_conf_parse_options(opts, doc, user, service))
    {
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return (0);
    }
	xmlFreeDoc(doc);
	xmlCleanupParser();
	return (1);
}
