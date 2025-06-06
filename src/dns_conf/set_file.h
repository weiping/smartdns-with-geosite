/*************************************************************************
 *
 * Copyright (C) 2018-2025 Ruilin Peng (Nick) <pymumu@gmail.com>.
 *
 * smartdns is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * smartdns is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _DNS_CONF_SET_FILE_H_
#define _DNS_CONF_SET_FILE_H_

#include "dns_conf.h"
#include "smartdns/dns_conf.h"

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus */

int _get_domain(char *value, char *domain, int max_domain_size, char **ptr_after_domain);

int _config_foreach_file(const char *file_pattern, int (*callback)(const char *file, void *priv), void *priv);

typedef int (*set_rule_add_func)(const char *value, void *priv);
int _config_domain_rule_each_from_geosite(const char *file, int type, set_rule_add_func callback, void *priv);
int _config_set_rule_each_from_list(const char *file, set_rule_add_func callback, void *priv);

#ifdef __cplusplus
}
#endif /*__cplusplus */
#endif
