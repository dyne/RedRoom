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

// Redis module for multi-threaded blocking operations using Zenroom.
// This uses Redis' experimental API and is still a work in progress.

#define REDISMODULE_EXPERIMENTAL_API
#include <redismodule.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include <zenroom.h>

#include <redis_namespace.h>

#define MAX_SCRIPT 8196
#define MAXOUT 4096

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

BLK *block_client(CTX *ctx);

zcmd_t *zcmd_init(CTX *ctx) {
	zcmd_t *zcmd = r_alloc(sizeof(zcmd_t)); // to be freed at end of thread!
	zcmd->stdout_len = MAXOUT; zcmd->stderr_len = MAXOUT; zcmd->error = 0;
	zcmd->scriptkey = NULL; zcmd->script = NULL;
	zcmd->destkey = NULL;
	zcmd->datakey = NULL; zcmd->data = NULL;
	zcmd->decscript = NULL;
	zcmd->bc = block_client(ctx);
	return(zcmd);
}

void *exec_tobuf(void *arg) {
	zcmd_t *zcmd = arg;
	CTX *ctx = RedisModule_GetThreadSafeContext(zcmd->bc);
	RedisModule_ThreadSafeContextLock(ctx);
	// execute script tobuf
	debug("exec script:\n%s",zcmd->decscript);
	if(zcmd->data)
		debug("exec data:\n%s",zcmd->data);
	if(zcmd->keys)
		debug("exec keys:\n%s",zcmd->keys);
	switch(zcmd->CMD) {
	case EXEC_LUA_TOBUF:
		zcmd->error = zenroom_exec_tobuf
			(zcmd->decscript, NULL, zcmd->keys, zcmd->data, 1,
			 zcmd->stdout_buf, MAXOUT, zcmd->stderr_buf, MAXOUT);
		break;
	case EXEC_ZENCODE_TOBUF:
		zcmd->error = zencode_exec_tobuf
			(zcmd->decscript, NULL, zcmd->keys, zcmd->data, 1,
			 zcmd->stdout_buf, MAXOUT, zcmd->stderr_buf, MAXOUT);
		break;
	}

	if(zcmd->decscript) r_free(zcmd->decscript);
	if(zcmd->data) r_closekey(zcmd->datakey);
	if(zcmd->keys) r_closekey(zcmd->keyskey);

	zcmd->stdout_len = strlen(zcmd->stdout_buf);
	zcmd->stderr_len = strlen(zcmd->stderr_buf);
	if(!zcmd->error) {
		if(!zcmd->destkey) {
			r_log(ctx, "notice", "ZENROOM.EXEC success:\n%s",
			      r_createstring(ctx, zcmd->stdout_buf, zcmd->stdout_len));
		} else {
			zcmd->error =
				r_stringset(zcmd->destkey, r_createstring(ctx, zcmd->stdout_buf, zcmd->stdout_len));
		}
	} else {
		if(!zcmd->destkey) {
			r_log(ctx, "warning", "ZENROOM.EXEC error:\n%s",
			      r_createstring(ctx, zcmd->stdout_buf, zcmd->stdout_len));
		} else {
			zcmd->error =
				r_stringset(zcmd->destkey, r_createstring(ctx, zcmd->stderr_buf, zcmd->stderr_len));
		}
	}

	// close, unlock and unblock after execution
	r_closekey(zcmd->destkey);
	RedisModule_ThreadSafeContextUnlock(ctx);
	RedisModule_FreeThreadSafeContext(ctx);
	r_unblockclient(zcmd->bc,zcmd);

	// zcmd is allocated by caller, freed by Zenroom_FreeData
	// all internal dynamic buffer allocations must be freed
	// at this point

	return NULL;
}

#define PCALLFMT "script = base64(\"%s\"); " \
	"res, f = pcall(loadstring, script:str()); " \
	"if(res) then pcall(f) else print \"ERROR\" end"

int zenroom_exectokey(CTX *ctx, STR **argv, int argc) {
	pthread_t tid;
	// we must have at least 2 args: SCRIPT DESTINATION
	if (argc < 2) return RedisModule_WrongArity(ctx);
	debug("argc: %u",argc);
	// ZENROOM.EXEC <script> <destination> [<data> <keys>]
	zcmd_t *zcmd = zcmd_init(ctx);

	// read scripts also from base64 encoded strings
	z_readargstr(ctx, 1, zcmd->script, zcmd->scriptkey, zcmd->scriptlen);
	if(!zcmd->script) {
		return r_replywitherror(ctx,"ZENROOM.EXEC: script string not found");
	}
	// uses zenroom to decode base64 and parse script before launching in protected mode
	zcmd->decscript = r_alloc(zcmd->scriptlen + strlen(PCALLFMT) + 16);
	snprintf(zcmd->decscript, MAX_SCRIPT, PCALLFMT, zcmd->script);
	r_closekey(zcmd->scriptkey); zcmd->script = NULL;

	if(argc > 2) {
		debug("%s: destination key", str(argv[2]));
		zcmd->destkey = r_openkey(ctx, argv[2], REDISMODULE_WRITE);
	}

	if(argc > 3) {
		debug("%s: data key", str(argv[3]));
		z_readargstr(ctx, 3, zcmd->data, zcmd->datakey, zcmd->datalen);
	}
	if(argc > 4) {
		debug("%s: keys key", str(argv[4]));
		z_readargstr(ctx, 4, zcmd->keys, zcmd->keyskey, zcmd->keyslen);
	}
	if (pthread_create(&tid, NULL, exec_tobuf, zcmd) != 0) {
		RedisModule_AbortBlock(zcmd->bc);
		r_free(zcmd); // reply not called from abort: free here
		return r_replywitherror(ctx,"-ERR Can't start thread");
	}
	return REDISMODULE_OK;

	// no command recognized
	return r_replywitherror(ctx,"ERR invalid ZENROOM command");

}

// main entrypoint symbol
int RedisModule_OnLoad(CTX *ctx) {
	// Register the module itself
	if (RedisModule_Init(ctx, "zenroom", 1, REDISMODULE_APIVER_1) ==
	    REDISMODULE_ERR)
		return REDISMODULE_ERR;
	if (RedisModule_CreateCommand(ctx, "zenroom.exec",
	                              zenroom_exectokey, "readonly",
	                              1, 1, 1) == REDISMODULE_ERR)
		return REDISMODULE_ERR;
	return REDISMODULE_OK;
}


int Zenroom_Reply(CTX *ctx, STR **argv, int argc) {
	STR *reply;
	switch(argc) {
	case 2:
		reply = r_createstringprintf
			(ctx, "ZENROOM.EXEC(%s)",
			 r_stringptrlen(argv[1], NULL));
		break;
	case 3:
		reply = r_createstringprintf
			(ctx, "%s = ZENROOM.EXEC(%s)",
			 r_stringptrlen(argv[2], NULL),
			 r_stringptrlen(argv[1], NULL));
		break;
	case 4:
		reply = r_createstringprintf
			(ctx, "%s = ZENROOM.EXEC(%s) ARG(%s)",
			 r_stringptrlen(argv[2], NULL),
			 r_stringptrlen(argv[1], NULL),
			 r_stringptrlen(argv[3], NULL));
		break;
	case 5:
		reply = r_createstringprintf
			(ctx, "%s = ZENROOM.EXEC(%s) ARG(%s) ARG(%s)",
			 r_stringptrlen(argv[2], NULL),
			 r_stringptrlen(argv[1], NULL),
			 r_stringptrlen(argv[3], NULL),
			 r_stringptrlen(argv[4], NULL));
		break;
	default:
		reply =	r_createstringprintf
			(ctx, "ZENROOM.EXEC wrong number of arguments (%u)", argc);
	}
	// return RedisModule_ReplyWithSimpleString(ctx,"OK");
	r_replywithstring(ctx,reply);
	r_freestring(ctx, reply);
	return REDISMODULE_OK;
}
int Zenroom_Timeout(CTX *ctx, STR **argv, int argc) {
	REDISMODULE_NOT_USED(argv); REDISMODULE_NOT_USED(argc);
	return r_replywithsimplestring(ctx,"Request timedout");
}
void Zenroom_FreeData(CTX *ctx, void *privdata) {
	REDISMODULE_NOT_USED(ctx);
	r_free(privdata);
}
void Zenroom_Disconnected(CTX *ctx, BLK *bc) {
	r_log(ctx,"warning","Blocked client %p disconnected!", (void*)bc);
	/* Here you should cleanup your state / threads, and if possible
	 * call RedisModule_UnblockClient(), or notify the thread that will
	 * call the function ASAP. */
}

BLK *block_client(CTX *ctx) {
	BLK *bc =
		RedisModule_BlockClient(ctx,
		                        Zenroom_Reply,
		                        Zenroom_Timeout,
		                        Zenroom_FreeData,
		                        3000); // timeout msecs
	RedisModule_SetDisconnectCallback(bc,Zenroom_Disconnected);
	return(bc);
}
