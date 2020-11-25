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

#include <redroom.h>

int reply_exectokey(CTX *ctx, STR **argv, int argc);

const char *STRPCALL = "" \
	"if (is_url64('%s')) then f=loadstring(url64('%s')); f()" \
	" elseif(is_base64('%s')) then f=loadstring(OCTET.from_base64('%s')); f()" \
	" else f=loadstring('%s'); f() end;";

// const char *B64PCALL =   "f = loadstring(base64('%s'):str()); f() ";

// const char *U64PCALL =   "f = loadstring(url64('%s'):str()); f() ";

/// ZENROOM.EXEC <script> <destination> [<data> <keys>]
int zenroom_exectokey(CTX *ctx, STR **argv, int argc) {
	pthread_t tid;
	// we must have at least 2 args: SCRIPT DESTINATION
	if (argc < 2) return RedisModule_WrongArity(ctx);
	debug("argc: %u",argc);
	int args = 0;
	zcmd_t *zcmd = zcmd_init(ctx);
	// read scripts also from base64 encoded strings
	debug("arg1: %s: script to exec", c_str(argv[1]));
	zcmd->script_k = r_openkey(ctx, argv[1], REDISMODULE_READ);
	size_t script_l;
	if(r_keytype(zcmd->script_k) != REDISMODULE_KEYTYPE_STRING) {
		r_log(ctx,"warning","arg 1 is expected to be a string");
	} else {
		zcmd->script_c = r_stringdma(zcmd->script_k,&script_l,REDISMODULE_READ);
	}
	// renders the script inside the wrapper, allocates the buffer
	zcmd->script = r_alloc(script_l + strlen(STRPCALL) + 16);
	snprintf(zcmd->script, MAX_SCRIPT, STRPCALL, zcmd->script_d);
	r_closekey(ctx, zcmd->script_k);

	args++;

	// save source of execution wrapper into an env variable
	KEY *src_k = r_openkey(ctx, r_str(ctx, "zenroom_exec_src"), REDISMODULE_WRITE);
	r_stringset(src_k, r_str(ctx, zcmd->script));
	r_closekey(src_k);

	if(argc > 2) {
		debug("arg2: %s: destination key", c_str(argv[2]));
		zcmd->dest_k = r_openkey(ctx, argv[2], REDISMODULE_WRITE);
	} else args++;

	if(argc > 3) {
		const char *_keys = c_str(argv[3]);
		debug("arg3: %s: keys", _keys);
		if(_keys == NULL || strncmp(_keys,"nil",3)==0) {
			debug("arg3: %s: keys (skip)","NULL");
		} else {
			zcmd->keyskey = r_openkey(ctx, argv[3], REDISMODULE_READ);
		}
	} else args++;

	if(argc > 4) {
		debug("arg4: %s: data key", c_str(argv[4]));
		zcmd->datakey = r_openkey(ctx, argv[4], REDISMODULE_READ);
	} else args++;
	zcmd->argc = args;
	return(zcmd);
}



int exec_tobuf(zcmd_t *zcmd) {
	CTX *ctx = zcmd->ctx;
//	CTX *ctx = RedisModule_GetThreadSafeContext(zcmd->bc);
//	RedisModule_ThreadSafeContextLock(ctx);
	// execute script tobuf
	debug("exec script:\n%s",zcmd->decscript);
	if(zcmd->datakey) {
		zcmd->data = r_stringdma(zcmd->datakey,&zcmd->datalen,REDISMODULE_READ);
		debug("exec data:\n%s",zcmd->data);
	}
	if(zcmd->keys) {
		zcmd->keys = r_stringdma(zcmd->keyskey, &zcmd->keyslen,REDISMODULE_READ);
		debug("exec keys:\n%s",zcmd->keys);
	}
	switch(zcmd->CMD) {
	case EXEC_LUA_TOBUF:
		zcmd->error = zenroom_exec_tobuf
			(zcmd->decscript, NULL, zcmd->keys, zcmd->data,
			 zcmd->stdout_buf, MAXOUT, zcmd->stderr_buf, MAXOUT);
		break;
	case EXEC_ZENCODE_TOBUF:
		zcmd->error = zencode_exec_tobuf
			(zcmd->decscript, NULL, zcmd->keys, zcmd->data,
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
//	RedisModule_ThreadSafeContextUnlock(ctx);
//	RedisModule_FreeThreadSafeContext(ctx);
//	r_unblockclient(zcmd->bc,zcmd);

	// zcmd is allocated by caller, freed by Zenroom_FreeData
	// all internal dynamic buffer allocations must be freed
	// at this point

	return NULL;

	zcmd_t *zcmd = z_readexecargs(ctx, argc, argv, zcmd);
	exec_tobuf(zcmd);
	if(zcmd->error)
		return REDISMODULE_ERR;
	else
		return REDISMODULE_OK;
}

int reply_exectokey(CTX *ctx, STR **argv, int argc) {
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
