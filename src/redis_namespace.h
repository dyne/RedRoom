
// get rid of the annoying camel-case in Redis, all its types are
// distinguished by being uppercase
typedef RedisModuleBlockedClient BLK;
typedef RedisModuleCtx           CTX;
typedef RedisModuleString        STR;
typedef RedisModuleKey           KEY;
typedef RedisModuleCallReply     REPLY;

#define r_log RedisModule_Log

// redis functions
#define r_alloc(p) RedisModule_Alloc(p)
#define r_calloc(n,p) RedisModule_Calloc(n,p)
#define r_free(p)  RedisModule_Free(p)

#define r_callreplytype RedisModule_CallReplyType
#define r_replyfree RedisModule_FreeCallReply
// redis log level strings:
// "debug"
// "verbose"
// "notice"
// "warning"

#define r_stringset(k, s) RedisModule_StringSet(k, s)
#define r_createstring(ctx,str,len) RedisModule_CreateString(ctx,str,len)
#define r_createstringprintf(ctx,format,args...) \
	RedisModule_CreateStringPrintf(ctx,format,args)
#define r_replywithstring(c,r) RedisModule_ReplyWithString(c,r)
#define r_freestring(c,r) RedisModule_FreeString(c,r)
#define r_stringptrlen(s,l) RedisModule_StringPtrLen(s,l)
#define r_replywithsimplestring(c,s) RedisModule_ReplyWithSimpleString(c,s)
#define r_replywitherror(ctx, str) RedisModule_ReplyWithError(ctx, str)

#define r_blockclient RedisModule_BlockClient
#define r_setdisconnectcallback RedisModule_SetDisconnectCallback
#define r_unblockclient(b,u) RedisModule_UnblockClient(b,u)

#define r_keytype(k) RedisModule_KeyType(k)
#define r_openkey(ctx, name, flag) RedisModule_OpenKey(ctx, name, flag)
#define r_closekey(k) RedisModule_CloseKey(k)
#define r_stringdma(key, len, flag) RedisModule_StringDMA(key, len, flag)

#define debug(fmt,args...) RedisModule_Log(ctx,"debug",fmt,args)
#define func(s) RedisModule_Log(ctx,"verbose",s)
#define str(s) RedisModule_StringPtrLen(s,NULL)

#define z_readargstr(ctx, argn, rdata, rkey, rlen) { \
	rkey = r_openkey(ctx, argv[argn], REDISMODULE_READ); \
	if(r_keytype(rkey) != REDISMODULE_KEYTYPE_STRING) { \
	    RedisModule_Log(ctx,"warning","arg %u is expected to be a string", argn); \
	    rdata = NULL; \
	} else { \
		rdata = r_stringdma(rkey,&rlen,REDISMODULE_READ); \
	} }

