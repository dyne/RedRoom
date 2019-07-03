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

const char *redroom_logo = "\n"
"   .-..``                                                                                                              \n"
"  `hs/+ooyo:`        :+::..                                                                                            \n"
"  `ho     .oy-       sy///osso/:`                  -/++//.        `/ysoso/.        `/oyyyy+-      .o+`       /+-       \n"
"  `yo       sy`     .hy      `.:`          `     `ss-. -ho       :y+    `+y.      :hs-`` -hy.     +h/y      /h.oo      \n"
"  +y/      -y+       oy.             `-/+soy+    :y/    sy-      +y`     `sy.     yh-     `sy:    +h y+     os  oo     \n"
"  +y/ ``.:ss-        /y.           .+y+-` `ho    `y+    -yo      yy       /y+     oy:      :yy`   /y--y+   `ys  `ho    \n"
"  oyyyo+/-.          /y:          /yo.     oy:    /y:   `ys     .yo       .yy     yy-       oy+   +y: .y+  /y.   +h`   \n"
"  +y-sy:             +ysoys+++// /y:       .y+     -+osshsy/    .yo        ys     oy/       /yo   oh   ss``so    .yo   \n"
"  oy` :ss.           +y+   `.--. oy-       .yy        -yo-yh`    yy.      .yo     :hy`      +yo  `yo   `ysos`     os   \n"
"  oy`  `+y+`         +yo         .yo        oy.       -y+ sy:    /y+      -y-      :yo      sy-  :yo    .yy-      .h+  \n"
"  +y`    .ss:        -ys          oy:      `+yo       +y- /y/     /y.    `+y-      .hy-   `-yy   /y/     `-        sh` \n"
"  sy`      /ys-      oyyoooossoo/ `:+o+oooss+/-      .ys  /yy     `/os++os+.        `oyyysyo:    yh`               .y+ \n"
"  yy`       `+:                       ````           +y/  `oo         ````             ````      yy`                :s-\n"
"  .:                                                 -/                                          `                     \n";

#include <redroom.h>

BLK *block_client(CTX *ctx);

// commands
int zenroom_exectokey(CTX *ctx, STR **argv, int argc);
int zenroom_setpwd(CTX *ctx, STR **argv, int argc);


zcmd_t *zcmd_init(CTX *ctx) {
	zcmd_t *zcmd = r_calloc(1,sizeof(zcmd_t)); // to be freed at end of thread!
	zcmd->stdout_len = MAXOUT; zcmd->stderr_len = MAXOUT;
	// memsetzcmd->error = 0;
	// zcmd->scriptkey = NULL; zcmd->script = NULL;
	// zcmd->destkey = NULL;
	// zcmd->datakey = NULL; zcmd->data = NULL;
	// zcmd->decscript = NULL;
	// zcmd->bc = block_client(ctx);
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
	if(zcmd->datakey) r_closekey(zcmd->datakey);
	if(zcmd->keyskey) r_closekey(zcmd->keyskey);

	zcmd->stdout_len = strlen(zcmd->stdout_buf);
	zcmd->stderr_len = strlen(zcmd->stderr_buf);
	if(zcmd->error && zcmd->destkey) {
		STR *out = r_createstring(ctx, zcmd->stderr_buf, zcmd->stderr_len);
		r_log(ctx, "warning", "ZENROOM.EXEC error:\n%s", zcmd->stderr_buf);
		r_stringset(zcmd->destkey, out);
	}
	if(zcmd->error && !zcmd->destkey) {
		r_log(ctx, "warning", "ZENROOM.EXEC error:\n%s", zcmd->stderr_buf);
	}
	if(!zcmd->error && !zcmd->destkey) {
		r_log(ctx, "notice", "ZENROOM.EXEC success:\n%s", zcmd->stdout_buf);
	}
	if(!zcmd->error && zcmd->destkey) {
		STR *out = r_createstring(ctx, zcmd->stdout_buf, zcmd->stdout_len);
		r_log(ctx, "verbose", "ZENROOM.EXEC success:\n%s", zcmd->stdout_buf);
		r_stringset(zcmd->destkey, out);
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

// ZENROOM.VERSION
int zenroom_version(CTX *ctx, STR **argv, int argc) {
	(void)argv;
	if (argc != 1) return RedisModule_WrongArity(ctx);
	r_replywithsimplestring(ctx, VERSION);
	return REDISMODULE_OK;
}

// main entrypoint symbol
int RedisModule_OnLoad(CTX *ctx) {
	// Register the module itself
	if (RedisModule_Init(ctx, "zenroom", 1, REDISMODULE_APIVER_1) ==
	    REDISMODULE_ERR)
		return REDISMODULE_ERR;
	// int RM_CreateCommand(RedisModuleCtx *ctx, const char *name,
	// RedisModuleCmdFunc cmdfunc, const char *strflags,
	// int firstkey, int lastkey, int keystep) see:
	// https://redis.io/commands/command
	if (RedisModule_CreateCommand(ctx, "zenroom.exec",
	                              zenroom_exectokey, "write",
	                              -2, 1, 1) == REDISMODULE_ERR)
		return REDISMODULE_ERR;
	if (RedisModule_CreateCommand(ctx, "zenroom.setpwd",
	                              zenroom_setpwd, "write",
	                              2, 1, 1) == REDISMODULE_ERR)
		return REDISMODULE_ERR;

	if (RedisModule_CreateCommand(ctx, "zenroom.version",
	                              zenroom_version,
	                              "", 0, 0, 0) == REDISMODULE_ERR)
		return REDISMODULE_ERR;

	fprintf(stdout, redroom_logo);
	r_log(ctx, "notice", "-> Redroom 0.1 powered by Zenroom %s", VERSION);
	return REDISMODULE_OK;
}

int default_reply(CTX *ctx, STR **argv, int argc) {
	REDISMODULE_NOT_USED(argv); REDISMODULE_NOT_USED(argc);
	return r_replywithsimplestring(ctx, "OK");
}

int default_timeout(CTX *ctx, STR **argv, int argc) {
	REDISMODULE_NOT_USED(argv); REDISMODULE_NOT_USED(argc);
	return r_replywithsimplestring(ctx,"Request timedout");
}
void default_freedata(CTX *ctx, void *privdata) {
	REDISMODULE_NOT_USED(ctx);
	r_free(privdata);
}
void default_disconnected(CTX *ctx, BLK *bc) {
	r_log(ctx,"warning","Blocked client %p disconnected!", (void*)bc);
	/* Here you should cleanup your state / threads, and if possible
	 * call RedisModule_UnblockClient(), or notify the thread that will
	 * call the function ASAP. */
}


