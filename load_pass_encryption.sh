#!/usr/bin/env zsh

zmodload zsh/system

load() {
	cat <<EOF | base64 -w0 | sysread encrypt
secrets = { text = 'my password', pin = '12345', salt = 'salt', iter = 10000 }
key = ECDH.pbkdf2(HASH.new('sha256'), str(secrets.pin), str(secrets.salt), secrets.iter, 32)
cipher = { iv = RNG.new():octet(16),
	       header = 'header' }
cipher.text, cipher.checksum =
   ECDH.aead_encrypt(key, str(secrets.text),
                     cipher.iv, str(cipher.header))
output = map(cipher, hex)
print(JSON.encode(output))
EOF
	echo "set encrypt \"$encrypt\"" | redis-cli

	cat <<EOF | base64 -w0 | sysread decrypt
cipher = map(JSON.decode(DATA),hex)
-- map(cipher,hex)
secrets = { text = 'my password', pin = '12345', salt = 'salt', iter = 10000 }
key = ECDH.pbkdf2(HASH.new('sha256'), str(secrets.pin), str(secrets.salt), secrets.iter, 32)
local decode = { header = cipher.header }
decode.text, checksum =
   ECDH.aead_decrypt(key, hex(cipher.text),
                     hex(cipher.iv), hex(cipher.header))
assert(checksum == cipher.checksum)
print(JSON.encode(map(decode, str)))
EOF
	echo "set decrypt \"$decrypt\"" | redis-cli
}

execute() {
	echo "zenroom exec encrypt encdest" | redis-cli
	echo "get encdest" | redis-cli

	echo "zenroom exec decrypt decdest encdest" | redis-cli
	echo "get decdest" | redis-cli
}

load

execute
