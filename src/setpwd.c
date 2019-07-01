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

/// ZENROOM.SETPWD <username> <password>
int zenroom_setpwd(CTX *ctx, STR **argv, int argc) {
	RedisModule_AutoMemory(ctx);
	// we must have at least 2 args: SCRIPT DESTINATION
	if (argc < 3) return RedisModule_WrongArity(ctx);
	debug("setpwd argc: %u",argc);
	debug("username: %s", str(argv[1]));
	zcmd_t *zcmd = zcmd_init(ctx);
	zcmd->bc = r_blockclient(ctx, default_reply,
	                         default_timeout, default_freedata, 3000);
	r_setdisconnectcallback(zcmd->bc,default_disconnected);

	KEY *username = r_openkey(ctx, argv[1], REDISMODULE_WRITE);
	char *password = (char*)r_stringptrlen(argv[2],&zcmd->keyslen);
	char *script = r_alloc(zcmd->keyslen + strlen(B64SHA512) + 16);
	snprintf(script, MAX_SCRIPT, B64SHA512, (char*)password);
	int error = zenroom_exec_tobuf
		(script, NULL, (char*)password, NULL, 1,
		 zcmd->stdout_buf, MAXOUT, zcmd->stderr_buf, MAXOUT);
	r_free(script);
	if(!error)
		r_stringset((KEY*)username,
		            r_createstring(ctx, zcmd->stdout_buf,
		                           strlen(zcmd->stdout_buf)));
	r_closekey((KEY*)username);
	if(error)
		return r_replywitherror(ctx,"ERROR: setpwd");
	r_replywithsimplestring(ctx,"OK");
	return REDISMODULE_OK;
}
