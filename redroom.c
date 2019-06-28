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
#include <base64.h>
#include <zenroom.h>

// get rid of the annoying camel-case in Redis, all its types are
// distinguished by being uppercase
typedef RedisModuleBlockedClient BLK;
typedef RedisModuleCtx           CTX;
typedef RedisModuleString        STR;
typedef RedisModuleKey           KEY;
// redis functions
#define r_alloc(p) RedisModule_Alloc(p)
#define r_free(p)  RedisModule_Free(p)

#define MAXOUT 4096

int Zenroom_Reply(CTX *ctx, STR **argv, int argc) {
	REDISMODULE_NOT_USED(argv);	REDISMODULE_NOT_USED(argc);
	STR *reply = RedisModule_CreateStringPrintf
		(ctx, "ZENROOM SET '%s' = EXEC(%s)", 
		 RedisModule_StringPtrLen(argv[2], NULL),
		 RedisModule_StringPtrLen(argv[3], NULL));
	// return RedisModule_ReplyWithSimpleString(ctx,"OK");
	RedisModule_ReplyWithString(ctx,reply);
	RedisModule_FreeString(ctx, reply);
	return REDISMODULE_OK;
}
int Zenroom_Timeout(CTX *ctx, STR **argv, int argc) {
	REDISMODULE_NOT_USED(argv);	REDISMODULE_NOT_USED(argc);
	return RedisModule_ReplyWithSimpleString(ctx,"Request timedout");
}
void Zenroom_FreeData(CTX *ctx, void *privdata) {
	REDISMODULE_NOT_USED(ctx);
	RedisModule_Free(privdata);
}
void Zenroom_Disconnected(CTX *ctx, BLK *bc) {
	RedisModule_Log(ctx,"warning","Blocked client %p disconnected!", (void*)bc);
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

// parsed command structure passed to execution thread
typedef enum { EXEC } zcommand;
typedef struct {
	BLK      *bc;   // redis blocked client
	zcommand  cmd;  // zenroom command (enum)
	KEY      *scriptkey; // redis key for script string
	char     *script;    // script string
	size_t    scriptlen; // length of script string
	KEY      *datakey;
	char     *data;
	size_t    datalen;
	KEY      *destkey;
	char     *dest;
	int error;
	char stdout_buf[MAXOUT];
	size_t stdout_len;
	char stderr_buf[MAXOUT];
	size_t stderr_len;
} zcmd_t;
void zcmd_teardown(zcmd_t *zcmd) {
	RedisModule_UnblockClient(zcmd->bc,zcmd);
}

void *thread_exec(void *arg) {
	zcmd_t *zcmd = arg;
	CTX *ctx = RedisModule_GetThreadSafeContext(zcmd->bc);
	RedisModule_ThreadSafeContextLock(ctx);
	// execute script tobuf
	char *descript = (char*)base64_dec_malloc(zcmd->script);
	RedisModule_CloseKey(zcmd->scriptkey);
	RedisModule_Log(ctx, "debug", "exec script:\n%s",descript);
	if(zcmd->data)
		RedisModule_Log(ctx, "debug", "exec data:\n%s",zcmd->data);
	zcmd->error = zenroom_exec_tobuf
		(descript, NULL, NULL, zcmd->data, 1,
		 zcmd->stdout_buf, MAXOUT, zcmd->stderr_buf, MAXOUT);
	RedisModule_Free(descript);
	if(zcmd->data)
		RedisModule_CloseKey(zcmd->datakey);
	zcmd->stdout_len = strlen(zcmd->stdout_buf);
	zcmd->stderr_len = strlen(zcmd->stderr_buf);
	// write result to dest
	if(!zcmd->error) {
		zcmd->error = RedisModule_StringSet(
			zcmd->destkey,
			RedisModule_CreateString(ctx, zcmd->stdout_buf, zcmd->stdout_len));
	} else {
		zcmd->error = RedisModule_StringSet(
			zcmd->destkey,
			RedisModule_CreateString(ctx, zcmd->stderr_buf, zcmd->stderr_len));
	}
	// close, unlock and unblock after execution
	RedisModule_CloseKey(zcmd->destkey);
	RedisModule_ThreadSafeContextUnlock(ctx);
	RedisModule_FreeThreadSafeContext(ctx);
	zcmd_teardown(zcmd);
	// zcmd is allocated by caller, freed by Zenroom_FreeData
	return NULL;
}

zcmd_t *zcmd_init(CTX *ctx) {
	zcmd_t *zcmd = r_alloc(sizeof(zcmd_t)); // to be freed at end of thread!
	zcmd->stdout_len = MAXOUT; zcmd->stderr_len = MAXOUT; zcmd->error = 0;
	zcmd->scriptkey = NULL; zcmd->script = NULL;
	zcmd->destkey = NULL;
	zcmd->datakey = NULL; zcmd->data = NULL;
	zcmd->bc = block_client(ctx);
	return(zcmd);
}

int Zenroom_Command(CTX *ctx, STR **argv, int argc) {
	pthread_t tid;
	size_t larg;
	const char *carg;
	// we must have at least 3 args: EXEC SCRIPT DESTINATION
	if (argc < 4) return RedisModule_WrongArity(ctx);

	// ZENROOM EXEC <script> <destination> [<data> <keys>]
	carg = RedisModule_StringPtrLen(argv[1], &larg);
	if (strncasecmp(carg,"EXEC",4) == 0) {
		zcmd_t *zcmd = zcmd_init(ctx);
		zcmd->cmd = EXEC;
		// get the script variable name from the next argument
		zcmd->scriptkey = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ);
		if (RedisModule_KeyType(zcmd->scriptkey) != REDISMODULE_KEYTYPE_STRING)
			return RedisModule_ReplyWithError(ctx, "ERR ZENROOM EXEC: no script found");
		zcmd->script = RedisModule_StringDMA(zcmd->scriptkey,&zcmd->scriptlen,REDISMODULE_READ);
		// destination
		zcmd->destkey = RedisModule_OpenKey(ctx, argv[3], REDISMODULE_WRITE);
		// optional arguments
		if(argc >=5) { // DATA
			zcmd->datakey = RedisModule_OpenKey(ctx, argv[4], REDISMODULE_READ);
			if (RedisModule_KeyType(zcmd->datakey) != REDISMODULE_KEYTYPE_STRING)
				return RedisModule_ReplyWithError(ctx, "ERR ZENROOM EXEC: no data found");
			zcmd->data = RedisModule_StringDMA(zcmd->datakey,&zcmd->datalen,REDISMODULE_READ);
		}
		if (pthread_create(&tid, NULL, thread_exec, zcmd) != 0) {
			RedisModule_AbortBlock(zcmd->bc);
			r_free(zcmd); // reply not called from abort: free here
			return RedisModule_ReplyWithError(ctx,"-ERR Can't start thread");
		}
		return REDISMODULE_OK;
	}

	// no command recognized
	return RedisModule_ReplyWithError(ctx,"ERR invalid ZENROOM command");

}

// main entrypoint symbol
int RedisModule_OnLoad(CTX *ctx) {
	// Register the module itself
	if (RedisModule_Init(ctx, "zenroom", 1, REDISMODULE_APIVER_1) ==
	    REDISMODULE_ERR)
		return REDISMODULE_ERR;
	if (RedisModule_CreateCommand(ctx, "zenroom",
	                              Zenroom_Command, "readonly",
	                              1, 1, 1) == REDISMODULE_ERR)
		return REDISMODULE_ERR;
	return REDISMODULE_OK;
}

