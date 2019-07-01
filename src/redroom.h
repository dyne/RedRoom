/* This file is part of RedRoom (https://zenroom.dyne.org)
 *
 * Copyright (C) 2019 Dyne.org foundation
 * designed, written and maintained by Denis Roio <jaromil@dyne.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef __REDROOM_H__
#define __REDROOM_H__

#define MAX_SCRIPT 8196
#define MAXOUT 4096

#define REDISMODULE_EXPERIMENTAL_API
#include <redismodule.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include <zenroom.h>

#include <redis_namespace.h>


#define STRPCALL         "f = loadstring('%s'); f()"	

#define B64PCALL   "f = loadstring(base64('%s'):str()); f() "


// parsed command structure passed to execution thread
typedef enum { EXEC_LUA_TOBUF, EXEC_ZENCODE_TOBUF } zcommand;
typedef struct {
	BLK      *bc;   // redis blocked client
	zcommand  CMD;  // zenroom command (enum)
	KEY      *scriptkey; // redis key for script string
	char     *script;    // script string
	size_t    scriptlen; // length of script string
	char     *decscript; // base64 decoded script
	KEY      *datakey;
	char     *data;
	size_t    datalen;
	KEY      *keyskey;
	char     *keys;
	size_t    keyslen;
	KEY      *destkey;
	char     *dest;
	int error;
	char stdout_buf[MAXOUT];
	size_t stdout_len;
	char stderr_buf[MAXOUT];
	size_t stderr_len;
} zcmd_t;

zcmd_t *zcmd_init(CTX *ctx);
void *exec_tobuf(void *arg);

int default_reply(CTX *ctx, STR **argv, int argc);
int  default_timeout(CTX *ctx, STR **argv, int argc);
void default_freedata(CTX *ctx, void *privdata);
void default_disconnected(CTX *ctx, BLK *bc);

#endif
