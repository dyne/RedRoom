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

/// ZENROOM.EXEC <script> <destination> [<data> <keys>]
int zenroom_exectokey(CTX *ctx, STR **argv, int argc) {
	pthread_t tid;
	// we must have at least 2 args: SCRIPT DESTINATION
	if (argc < 2) return RedisModule_WrongArity(ctx);
	debug("argc: %u",argc);
	zcmd_t *zcmd = zcmd_init(ctx);
	zcmd->bc = r_blockclient(ctx, reply_exectokey,
	                         default_timeout, default_freedata, 3000);
	r_setdisconnectcallback(zcmd->bc,default_disconnected);

	// read scripts also from base64 encoded strings
	debug("arg1: %s: script to exec", str(argv[1]));
	z_readargstr(ctx, 1, zcmd->script, zcmd->scriptkey, zcmd->scriptlen);
	if(!zcmd->script) {
		return r_replywitherror(ctx,"ZENROOM.EXEC: script string not found");
	}
	// uses zenroom to decode base64 and parse script before launching in protected mode
	zcmd->decscript = r_alloc(zcmd->scriptlen + strlen(B64PCALL) + 16);
	snprintf(zcmd->decscript, MAX_SCRIPT, B64PCALL, zcmd->script);
	r_closekey(zcmd->scriptkey); zcmd->script = NULL;

	if(argc > 2) {
		debug("arg2: %s: destination key", str(argv[2]));
		zcmd->destkey = r_openkey(ctx, argv[2], REDISMODULE_WRITE);
	}

	if(argc > 3) {
		const char *_data = str(argv[3]);
		if(_data == NULL || strncmp(_data,"nil",3)==0) {
			debug("arg3: %s: data key (skip)","NULL");
			zcmd->data = NULL;
		} else {
			debug("arg3: %s: data key", _data);
			z_readargstr(ctx, 3, zcmd->data, zcmd->datakey, zcmd->datalen);
		}
	}
	if(argc > 4) {
		debug("arg4: %s: keys key", str(argv[4]));
		z_readargstr(ctx, 4, zcmd->keys, zcmd->keyskey, zcmd->keyslen);
		if(!zcmd->keys)
			return r_replywitherror(ctx,"-ERR argument 4 is null");			
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
