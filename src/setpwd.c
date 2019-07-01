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

#define B64SHA512 "write(ECDH.kdf(HASH.new('sha512'),'%s'):base64())"

/// ZENROOM.SETPWD <username> <password>
int zenroom_setpwd(CTX *ctx, STR **argv, int argc) {
	char stdout_buf[512];
	char stderr_buf[512];
	char script[64];
	// RedisModule_AutoMemory(ctx);
	// we must have at least 2 args: SCRIPT DESTINATION
	if (argc < 3) return RedisModule_WrongArity(ctx);
	debug("setpwd argc: %u",argc);
	debug("username: %s", str(argv[1]));

	char *password = (char*)r_stringptrlen(argv[2],NULL);
	debug("password: %s",password);
	// char *script = r_alloc(zcmd->keyslen + strlen(B64SHA512) + 16);
	snprintf(script, MAX_SCRIPT, B64SHA512, (char*)password);
	int error = zenroom_exec_tobuf
		(script, NULL, (char*)password, NULL, 1,
		 stdout_buf, 512, stderr_buf, 512);
	// r_free(script);
	if(error) return r_replywitherror(ctx,"ERROR: setpwd");
	REPLY *reply;
	reply = RedisModule_Call(ctx,"SET","ss", argv[1],
	                         r_createstring(ctx, stdout_buf, strlen(stdout_buf)));
	if (r_callreplytype(reply) == REDISMODULE_REPLY_ERROR) {
		RedisModule_ReplyWithCallReply(ctx, reply);
		r_replyfree(reply);
		return REDISMODULE_ERR;
	}
	if (r_callreplytype(reply) == REDISMODULE_REPLY_NULL ) {
		RedisModule_ReplyWithCallReply(ctx, reply);
		r_replyfree(reply);
		return REDISMODULE_ERR;
	}
	return r_replywithsimplestring(ctx,"OK");
}
