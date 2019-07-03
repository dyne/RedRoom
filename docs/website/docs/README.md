# Easy crypto for Redis

RedRoom is powered by the [Zenroom crypto VM](https://zenroom.dyne.org) to bring easy to use yet advanced **cryptographic functions in Redis**.

This software is in **ALPHA** stage and published for preview.

The main use-case covered is that of **secure password storage** using hashes inside username keys and check if password matches.

Future plans and low hanging fruits:

- **authenticated private messaging** using encrypted PUB/SUB channels with asymmetric keypairs
- **brute-force resistant hashing** with alternatives to SHA512 that cannot run on GPUs

We are open to more ideas: don't hesitate to show us your interest, it motivates us!


## <span class="mdi mdi-raspberry-pi turq"></span> Supported platforms

<p>
RedRoom is developed and tested on Linux, Windows and Mac OS.
</p>

<p>
RedRoom runs fine on ARM, i386 and x86_64 CPUs.
</p>

The <a href="https://zenroom.dyne.org">Zenroom</a> crypto engine at
the core of RedRoom is portable to:
<ul style="list-style: none">
<li><span class="mdi mdi-apple"></span> native iOS framework
<li><span class="mdi mdi-android"></span> native Android library
<li><span class="mdi mdi-language-javascript"></span> Javascript and WebAssembly
<li><span class="mdi mdi-chip"></span> Cortex chips
<li> ... even more targets
</ul>

<script id="asciicast-255267" src="https://asciinema.org/a/255267.js" async></script>

## <span class="mdi mdi-textbox has-text-link" ></span> Commands

All commands provided by Redroom are prefixed with `ZENROOM.` or `ZENCODE.`

### ZENROOM.EXEC

```
ZENROOM.EXEC SCRIPT DESTINATION [ KEYS DATA ]
```

Execute the contents stored in key `SCRIPT` using the Zenroom VM language based on Lua, then stores the result in key `DESTINATION`; the execution is passed two arguments, the contents of keys `KEYS` and `DATA`.

### ZENCODE.EXEC

```
ZENCODE.EXEC ZENCODE DESTINATION [ KEYS DATA ]
```

Execute the human language instructions stored in the key `ZENCODE` using the Zenroom VM, then stores the result in key `DESTINATION`; the execution is passed two arguments, the contents of keys `KEYS` and `DATA`. For more information on the human language used see [Zencode: Smart contracts for the English speaker](https://decodeproject.eu/blog/smart-contracts-english-speaker).


### ZENROOM.SETPASS

```
ZENROOM.SETPASS USERNAME PASSWORD
```

Safely stores the string `PASSWORD` hashed using SHA512 and KDF inside key `USERNAME` (base64 encoded). Username keys will not contain actual password strings, but hashes that are only useful to verify if the password given at a login is correct, using `ZENROOM.CHECKPASS`.

The Zenroom code used is: `write(ECDH.kdf(HASH.new('sha512'),'%s'):base64())`

### ZENROOM.CHECKPASS

```
ZENROOM.CHECKPASS USERNAME PASSWORD
```

Checks that the key `USERNAME` is existing and its contents match the `PASSWORD` string when hashed using SHA512 and KDF.

## BENCHMARK

A port of redis-benchmark is provided and builds with target `make check`.

Performance of `ZENROOM.SETPASS` can be tested with:

```
./benchmark -t zenroom.setpwd -n 1000 -r 1000
```

On a fifth gen i5 (2.4GHz) running Redis 5 leads to approximately 222 requests per second, each one executing several SHA512 based KDF iterations on each string.

![RedRoom logo](https://redroom.dyne.org/img/redroom-trans.png)


## Acknowledgements

RedRoom is Copyright (C) 2019 by the [Dyne.org](https://www.dyne.org) foundation

Written and maintained by Denis Roio <jaromil@dyne.org> and co-designed with Andrea D'Intino

## License

    RedRoom is Copyright (c) 2019 by the Dyne.org foundation
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.
    
    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
