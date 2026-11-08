# Third-party components

Flow Go uses GPL-3.0 as its project license. Existing upstream notices remain
in the vendored sources. This document is an attribution index, not a replacement
for those license texts.

| Component | Source / notice |
|---|---|
| PSPDEV / PSPSDK | https://github.com/pspdev/pspsdk — external build dependency |
| pkg2iso / PKGi helpers | https://github.com/bucanero/pkgi-psp — GPL-3.0; modified transactional I/O, bounds checks and PSP-only installation |
| pkg2zip / LZRC ancestry | mmozeiko/pkg2zip and tpu/libkirk, notices retained in source |
| tiny-AES-c | https://github.com/kokke/tiny-AES-c — `vendor/TINY-AES-LICENSE.txt` |
| SHA-256 | https://github.com/B-Con/crypto-algorithms — upstream public-domain notice |
| Mbed TLS 3.6.7 | https://github.com/Mbed-TLS/mbedtls — `licenses/MbedTLS-LICENSE.txt` |
| TileFinch PSP entropy | `licenses/Tilefinch-LICENSE.txt`; adapted integration in `vendor/entropy` |
| MiniZip, zlib 1.3.1 | https://github.com/madler/zlib — `licenses/MINIZIP.txt`; zlib linked from toolchain |
| jsmn | https://github.com/zserge/jsmn — `licenses/jsmn-LICENSE.txt` |
| FreeType | https://freetype.org — FreeType License, `licenses/FreeType-*` |
| libpng | `licenses/libpng-LICENSE.txt` — linked from toolchain |
| bzip2 | External toolchain dependency; upstream bzip2 license |
| Noto Sans / Noto Sans JP | `assets/OFL.txt`, `assets/FONT-JP-LICENSE.txt` — SIL OFL |
| CA bundle | https://curl.se/ca/cacert.pem — Mozilla collection; MPL-2.0 |
| PSPDEV Homebrew database | https://github.com/pspdev/homebrew — remote metadata, Unlicense |

Artwork for commercial games and downloaded game files are not included in this
public distribution. The ambient loop generator is original project material;
the generated loop is offered under CC0 as described in MUSIC.txt in the release.
