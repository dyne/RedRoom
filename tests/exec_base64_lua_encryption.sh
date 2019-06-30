#!/usr/bin/env zsh

zmodload zsh/system

testrounds=3

cat <<EOF | sysread secretgen
secret = { text = 'test secret',
	       pin =   RNG.new():octet(16),
		   salt = str('buuZiaquaeth7yiepei0ieCh7chu1soh'),
		   iter = 10000 }
write(JSON.encode(map(secret,base64)))
EOF

cat <<EOF | base64 -w0 | sysread encrypt
secrets = JSON.decode(KEYS, base64)
key = ECDH.pbkdf2(HASH.new('sha256'), secrets.pin, secrets.salt, secrets.iter, 32)
cipher = { iv = RNG.new():octet(16), header = 'header' }
cipher.text, cipher.checksum =
   ECDH.aead_encrypt(key, secrets.text,
                     cipher.iv, str(cipher.header))
output = map(cipher, base64)
write(JSON.encode(output))
EOF

cat <<EOF | base64 -w0 | sysread decrypt
secrets = JSON.decode(KEYS, base64)
cipher = map(JSON.decode(DATA),base64)
key = ECDH.pbkdf2(HASH.new('sha256'), secrets.pin, secrets.salt, secrets.iter, 32)
local decode = { header = cipher.header }
decode.text, checksum =
   ECDH.aead_decrypt(key, hex(cipher.text),
                     hex(cipher.iv), hex(cipher.header))
assert(checksum == cipher.checksum)
print(JSON.encode(map(decode, str)))
EOF

newsecret_redis() {
	echo "set ${1:-secret} '`echo $secretgen | zenroom`'"
}

newsecret_local() {
	echo $secretgen | zenroom > secret.keys
	cat secret.keys
	echo
}

load_redis() {
	echo "set encrypt \"b64:$encrypt\"" | redis-cli
	echo "set decrypt \"b64:$decrypt\"" | redis-cli
}

execute_redis() {

	echo "zenroom.exec encrypt encoded nil secret" | redis-cli
	echo "get encoded" | redis-cli
	
	echo "zenroom.exec decrypt decoded encoded secret" | redis-cli
	echo "get decoded" | redis-cli

}

execute_local() {
	echo "$encrypt" | base64 -d | zenroom -k secret.keys | tee encoded.data
	echo
	echo "$decrypt" | base64 -d | zenroom -a encoded.data -k secret.keys
	echo
}

newsecret_local
execute_local

load_redis
newsecret_redis
execute_redis
