
// get rid of the annoying camel-case in Redis, all its types are
// distinguished by being uppercase
typedef RedisModuleBlockedClient BLK;
typedef RedisModuleCtx           CTX;
typedef RedisModuleString        STR;
typedef RedisModuleKey           KEY;
// redis functions
#define r_alloc(p) RedisModule_Alloc(p)
#define r_free(p)  RedisModule_Free(p)
#define r_log(c,t,f,args...) RedisModule_Log(c,t,f,args)

#define r_stringset(k, s) RedisModule_StringSet(k, s)
#define r_createstring(ctx,str,len) RedisModule_CreateString(ctx,str,len)
#define r_createstringprintf(ctx,format,args...) \
	RedisModule_CreateStringPrintf(ctx,format,args) 
#define r_replywithstring(c,r) RedisModule_ReplyWithString(c,r)
#define r_freestring(c,r) RedisModule_FreeString(c,r)
#define r_stringptrlen(s,l) RedisModule_StringPtrLen(s,l)
#define r_replywithsimplestring(c,s) RedisModule_ReplyWithSimpleString(c,s)
#define r_replywitherror(ctx, str) RedisModule_ReplyWithError(ctx, str)

#define r_blockclient(ctx,reply_f,timeout_f,free_f,ttl) \
	RedisModule_BlockClient(ctx,reply_f,timeout_f,free_f,ttl)
#define r_unblockclient(b,u) RedisModule_UnblockClient(b,u)

#define r_keytype(k) RedisModule_KeyType(k)
#define r_openkey(ctx, name, flag) RedisModule_OpenKey(ctx, name, flag)
#define r_closekey(k) RedisModule_CloseKey(k)
#define r_stringdma(key, len, flag) RedisModule_StringDMA(key, len, flag)
