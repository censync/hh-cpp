# Integration

How to put hh into an application: what to hash, which mode to show where, and recipes for C++,
Qt, the C ABI and JNI.

## 1. Recommended inputs

hh hashes bytes, never display strings, so that every spelling of one address gives one picture.
Independent hosts agree on the picture only if they agree on the bytes:

| Family | Input | API |
|---|---|---|
| EVM chains | the 20 address bytes | `make_base_digest`, or `make_base_digest_from_hex` with the usual `0x...` text; EIP-55, lower and upper case give the same picture |
| Sui, Aptos | the 32 address bytes, left-padded | binary |
| Solana | the 32 public-key bytes | binary |
| Tron | 21 bytes: `41` and the 20 address bytes | binary |
| TON | 36 bytes: the workchain as 4 bytes big-endian, then the 32-byte hash, so that bounceable and non-bounceable forms match | binary |
| Bitcoin and other text-only formats | the address text verbatim, bech32 in lower case | `make_base_digest_from_text` |
| Transaction and block hashes | the raw hash bytes | binary |

The chain is deliberately not mixed in: one EVM address is one identity on every EVM chain. A
binary input and a text input with the same bytes give different pictures.

## 2. The three steps and what to cache

```
input --make_base_digest--> base digest --*_fingerprint--> fingerprint --render--> pixels
        slow, public              one HMAC at most                   fast
```

- The **base digest** costs about 16 000 HMAC calls: 6 to 8 ms on a desktop core, 12 to 35 ms on a
  phone. Compute it off the UI thread and cache the 32 bytes per address
  (`base_digest::bytes()`, `import_base_digest`). It is public; it needs no protection.
- The **fingerprint** is the digest itself in universal mode and one HMAC in keyed mode. Changing
  the key or the mode never needs the slow step again.
- **Rendering** a 128-pixel picture takes about a third of a millisecond; cache pixels per size
  if lists scroll.

## 3. Product rules

1. Keyed is the default for everything the application shows to its own user: address book,
   history, send confirmation, paste check.
2. Universal is for parties that do not hold the key: the receive screen shown to a payer, share
   and export, payment requests, support. Exported or shared pictures are never keyed.
3. One picture per address per screen. A deliberate toggle may swap to the other mode together
   with its caption ("Private picture - only this wallet shows it" / "Public picture - the same
   for everyone"); never both side by side.
4. A new recipient: show the universal picture to compare with the payee's own; on saving, say
   that the private picture is shown from now on.
5. Two devices of one user show keyed pictures only if they hold the same key; otherwise both
   fall back to universal.
6. Size: a picture that backs a decision is at least 64 dp, better 96 dp, for the square shape,
   and about a third larger for the round shape, whose cells are smaller. It stands beside the
   picture it is compared with. Smaller renders (32 to 48 dp in list rows) are for recognition
   and never the basis of an approval.
7. Offer the tag (`K7Q-M2X`) wherever the user can compare text: it is certain where a picture is
   not.
8. When the key check value differs from the stored one, every private picture is about to
   change: stop and explain, do not re-key silently.

## 4. Looks

`render_options` chooses the look; the cells, the palette and the geometry are fixed.

- `shape`: `square` (default) or `round` (the grid inscribed in a circle).
- `frame`: `automatic` gives universal pictures no frame and keyed square pictures rounded
  corners. `none` and `plain` are open to both modes; `rounded`, `chamfered`, `double_line`,
  `thick`, `brackets` (square) and `double_line`, `thick`, `ticks`, `gaps` (round) mark a keyed
  picture and are refused for a universal fingerprint. Use one style everywhere in an
  application and on every device of a user: the marker is only useful if it is familiar.
- The round shape has no marker by default, so there the caption alone names the mode.
- `background` and `background_alpha`: any colour, from transparent to opaque. Outside rounded
  or chamfered corners and outside the disc the picture is always transparent.
- `frame_alpha`: the frame colour is fixed (`808080`), its transparency is not.
- Contrast: `render` refuses an opaque background with less than 2:1 against any palette colour
  (`low_contrast`). `measure_contrast(options, page)` reports the WCAG ratio times 100 for the
  figures and for the frame; warn the user below 300. For a translucent background pass the
  colour of the surface underneath as `page`.
- On a dark theme use a transparent background over a dark surface (`121212` or darker) or an
  opaque dark background; avoid mid greys and saturated surfaces.

## 5. Recipes

### C++

```cpp
#include <hh/hh.hpp>

hh::error_code picture(std::string_view address_hex, const hh::secret_key* key, std::uint32_t size,
                       hh::image& out) {
    hh::base_digest digest;                       // cache digest.bytes() per address
    hh::error_code ec = hh::make_base_digest_from_hex(address_hex, digest);
    if (ec != hh::error_code::ok) return ec;
    hh::fingerprint fp;
    ec = key != nullptr ? hh::keyed_fingerprint(digest, *key, fp)
                        : hh::universal_fingerprint(digest, fp);
    if (ec != hh::error_code::ok) return ec;
    return hh::render(fp, size, hh::render_options{}, out);
}
```

Nothing throws; every function returns an `error_code`, and `error_message` gives an English
text for logs.

### Qt

```cpp
hh::image img;
hh::render(fp, static_cast<std::uint32_t>(side * devicePixelRatioF()), options, img);
QImage view(img.rgba.data(), int(img.width), int(img.height), int(img.width) * 4,
            QImage::Format_RGBA8888);            // straight alpha, no copy
QImage owned = view.copy();                      // detach before img goes away
owned.setDevicePixelRatio(devicePixelRatioF());
```

Render at the exact device-pixel size instead of scaling: the rasteriser is anti-aliased for the
size it is asked for. With qmake, build the static library with CMake and add
`INCLUDEPATH += $$PWD/hh/include` and `LIBS += -L<build dir> -lhh`; the archive lands in the root
of the build directory.

### C ABI

`include/hh/hh.h` is plain C: fixed-size byte arrays, integers, caller-allocated buffers, one
`hh_error` per call. Encoders follow the usual size query:

```c
uint8_t digest[HH_DIGEST_SIZE], fp[HH_FINGERPRINT_SIZE];
hh_base_digest(address, 20, digest);
hh_keyed_fingerprint(digest, key32, fp);          /* or hh_universal_fingerprint */

size_t bytes = hh_render_bytes(128);
uint8_t* rgba = malloc(bytes);
hh_render(fp, HH_MODE_KEYED, 128, NULL, rgba, bytes);

size_t png_size = 0;
hh_encode_png(rgba, 128, 128, NULL, 0, &png_size);      /* HH_BUFFER_TOO_SMALL, png_size set */
uint8_t* png = malloc(png_size);
hh_encode_png(rgba, 128, 128, png, png_size, &png_size);
```

### JNI

Keep the key on the native side and hand only pixels to the JVM:

```cpp
JNIEXPORT jintArray JNICALL Java_example_Hh_render(JNIEnv* env, jclass, jbyteArray fp32, jint mode,
                                                  jint size) {
    jbyte fp[HH_FINGERPRINT_SIZE];
    env->GetByteArrayRegion(fp32, 0, HH_FINGERPRINT_SIZE, fp);
    std::vector<uint8_t> rgba(hh_render_bytes(uint32_t(size)));
    if (rgba.empty() || hh_render(reinterpret_cast<const uint8_t*>(fp), mode, uint32_t(size),
                                  nullptr, rgba.data(), rgba.size()) != HH_OK) {
        return nullptr;
    }
    std::vector<jint> argb(rgba.size() / 4);      // Bitmap.createBitmap wants straight ARGB ints
    for (size_t i = 0; i < argb.size(); ++i) {
        argb[i] = jint(uint32_t(rgba[4 * i + 3]) << 24 | uint32_t(rgba[4 * i]) << 16 |
                       uint32_t(rgba[4 * i + 1]) << 8 | rgba[4 * i + 2]);
    }
    jintArray out = env->NewIntArray(jsize(argb.size()));
    env->SetIntArrayRegion(out, 0, jsize(argb.size()), argb.data());
    return out;
}
```

A host that computes the keyed HMAC in a secure element or in another process passes the 32
result bytes to `import_fingerprint` (C ABI: any function that takes a fingerprint and a mode)
and renders with a library that never sees the key. The other implementations offer the same:
`Fingerprint.fromBytes` in hh-kotlin and hh-ts, `ImportFingerprint` in go-hh,
`Fingerprint.from_bytes` in hh-python.

## 6. Threads and memory

The library keeps no global state; every function may be called from any thread. `render`
allocates `size * size * 4` bytes, `render_into` and the C ABI allocate nothing for rendering.
Encoders allocate their output, and PNG a working copy of the raw stream (about the size of the
pixels); they report `out_of_memory` instead of throwing. The C ABI encodes straight from the
caller's pixels; its size query (a null output buffer) costs a full encode, so for large images
allocate generously instead of asking twice.

## 7. Conformance

An implementation or a binding conforms if it reproduces every record of
`testdata/vectors.tsv` and every file of `testdata/golden/` (SPEC.md section 15).
`gen_vectors --check testdata` proves that for this library; `hh_cli --batch` is the hook for
differential tests against another implementation.
