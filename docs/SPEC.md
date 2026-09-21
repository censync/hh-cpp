# Humanized Hash (`hh`) - specification

Status: normative. This document defines the algorithm completely: two implementations that
follow it produce identical fingerprints, identical pixels and identical PNG, BMP and JPEG files.
The reference implementation is hh-cpp; its golden vectors (`testdata/`, section 15) are part of
the specification. If the text and the vectors disagree, that is a defect of this document and
the vectors win.

The algorithm is frozen and carries no version: a picture is only useful if it stays the same
for ever, so no constant, formula or encoding defined here will change. Libraries that implement
it have their own release versions, which never affect the output.

The key words MUST, MUST NOT, SHOULD and MAY are used as in RFC 2119.

## 1. Overview

```
data --canonicalise--> M1 --SHA-256--> d0 --PBKDF2--> s  (base digest, public, slow)
s ----------------------------------------------> fp     (universal mode)
s --HMAC-SHA-256 with the 32-byte key-----------> fp     (keyed mode)
fp --> 16 cells + tag --> render(size, options) --> RGBA pixels --> PNG | BMP | JPEG
```

- The base digest `s` is the only slow value. Hosts cache it per input; both modes reuse it.
- Universal mode is computable by anyone. Keyed mode needs a 32-byte secret key; without the key
  the image of an input cannot be computed or predicted.
- The two fingerprints of one input are unrelated, so the two modes give unrelated images.

## 2. Notation

- A byte is an integer in 0..255. Byte strings are written in hexadecimal; `||` concatenates.
- `u16be(n)` and `u32be(n)` are the 2-byte and 4-byte big-endian encodings of `n`, `u16le` and
  `u32le` the little-endian ones.
- `floor(a / b)` is division rounded towards negative infinity. Unless stated otherwise every
  division in this document has non-negative operands, where it equals truncating division. In
  formulas with integer operands a bare `/` means the same floor division.
- `min`, `max` and `abs` have their usual meaning. `clamp(v, lo, hi) = min(max(v, lo), hi)`.
- All arithmetic is integer arithmetic. No step of the algorithm uses floating point.
- ASCII strings in double quotes denote their bytes without a terminator.
- SHA-256 is FIPS 180-4; HMAC-SHA-256 is RFC 2104 over SHA-256; PBKDF2-HMAC-SHA-256 is RFC 8018
  section 5.2 with HMAC-SHA-256 as the PRF.

## 3. Input canonicalisation

The algorithm hashes bytes, never display strings. An input has a kind and a byte string `data`:

| Kind | Byte | Meaning |
|---|---|---|
| binary | `00` | raw bytes (an address, a public key, a hash) |
| text | `01` | the UTF-8 encoding of a string, taken verbatim |

- `data` MUST be 1 to 1 048 576 bytes long. An empty input is the error `empty_input`, a longer
  one `input_too_large`.
- Hexadecimal input is a convenience form of a binary input: an optional prefix `0x` or `0X`
  followed by an even, non-zero number of characters from `0-9`, `a-f`, `A-F`. It decodes to the
  bytes it denotes, most significant nibble first. Anything else (odd length, other characters,
  white space, a bare prefix) is the error `invalid_hex`. The length limits apply to the decoded
  bytes and are checked after the syntax: an overlong string with a bad character is
  `invalid_hex`. Upper and lower case, with or without the prefix, give the same input.
- A text input is encoded as UTF-8 without a byte order mark, without normalisation and without
  a terminator. An interface that takes a string which can hold ill-formed sequences (for
  example unpaired UTF-16 surrogates) MUST reject them with `invalid_argument` instead of
  replacing them, after the length check; for that check an unpaired surrogate counts as three
  bytes. An interface that takes the UTF-8 bytes takes them verbatim and does not validate them.
- A binary input and a text input with equal bytes are different inputs.

Which bytes represent an address is the host's choice and is outside this algorithm. Section
"Recommended inputs" of `INTEGRATION.md` lists the choices that make independent hosts agree.

## 4. Derivation

```
DST = "HumanizedHash"                                              (13 bytes)
M1  = DST || 00 || 01 || kind || u32be(len(data)) || data
d0  = SHA-256(M1)
s   = PBKDF2-HMAC-SHA-256(P = d0, S = "HumanizedHash/stretch", c = 16384, dkLen = 32)

universal:  fp = s
M2  = DST || 00 || 02 || s                                         (47 bytes)
keyed:      fp = HMAC-SHA-256(K, M2)
KCV = the first 4 bytes of HMAC-SHA-256(K, DST || 00 || 03)
```

- The two bytes after `DST` are a separator and the stage (`01` digest, `02` keyed fingerprint,
  `03` key check value); they keep the three uses of the hash apart.
- `s`, the base digest, is 32 bytes and public.
- The key `K` MUST be exactly 32 bytes and MUST NOT be all zero; otherwise the error is
  `invalid_key`. There is no passphrase form: `K` must be uniformly random or the output of a
  key derivation function.
- `KCV`, the key check value, lets a host detect that the key changed. It reveals nothing useful
  about `K`.
- A fingerprint `fp` is 32 bytes together with its mode (universal = 1, keyed = 2). A host that
  computes the keyed HMAC elsewhere MAY import the 32 bytes with the mode.

## 5. Features

### 5.1 Cells

The image is a 4 x 4 matrix. Cell `i` (row-major, row `floor(i / 4)`, column `i mod 4`, row 0 at the
top, column 0 at the left) is defined by byte `b = fp[i]`, `i` = 0..15:

| Bits of `b` | Field | Value |
|---|---|---|
| 7..5 | figure code | `floor(b / 32)` |
| 4..3 | colour index | `floor(b / 8) mod 4` |
| 2..0 | reserved | ignored |

| Figure code | Figure | Layout value |
|---|---|---|
| 0, 1 | none (empty cell) | 0 |
| 2 | square | 1 |
| 3 | circle | 2 |
| 4 | triangle pointing up | 3 |
| 5 | triangle pointing right | 4 |
| 6 | triangle pointing down | 5 |
| 7 | triangle pointing left | 6 |

The colour index of an empty cell has no meaning; the layout output reports it as 0.
Bytes 20..31 of `fp` are not used.

### 5.2 Palette

| Colour index | sRGB |
|---|---|
| 0 | `7A96C5` |
| 1 | `890AF0` |
| 2 | `C10445` |
| 3 | `D48200` |

The frame colour is `808080`. All colours are sRGB triples `RRGGBB`.

### 5.3 Tag

The tag is a 6-character text form of 30 further bits, for hosts that want a check that is
certain where a picture is not:

```
word = fp[16] * 2^24 + fp[17] * 2^16 + fp[18] * 2^8 + fp[19]
v    = floor(word / 4)                                  (30 bits)
tag  = A[floor(v / 2^25) mod 32] A[floor(v / 2^20) mod 32] ... A[v mod 32]
A    = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"               (Crockford Base32)
```

Hosts SHOULD display it grouped as `XXX-XXX`. The tag of a keyed fingerprint depends on the key.

## 6. Rendering options

A render takes a fingerprint, a size `S` and these options:

| Option | Values | Default |
|---|---|---|
| shape | `square`, `round` | `square` |
| frame | `automatic`, `none`, `plain`, `rounded`, `chamfered`, `double`, `thick`, `brackets`, `ticks`, `gaps` | `automatic` |
| background | an sRGB colour `B` and an alpha `ab` in 0..255 | `FFFFFF`, 255 |
| frame alpha | `af` in 0..255 | 255 |

`automatic` resolves to `rounded` for a keyed fingerprint with the square shape and to `none`
otherwise. `none` draws no frame; `plain` draws the plain frame of the shape (a thin square or a
thin ring). Every other style is a keyed-mode marker: it tells a user that the picture is the
private one.

| Style | Square | Round | Universal | Keyed |
|---|---|---|---|---|
| `none`, `plain` | yes | yes | yes | yes |
| `rounded`, `chamfered`, `brackets` | yes | no | no | yes |
| `double`, `thick` | yes | yes | no | yes |
| `ticks`, `gaps` | no | yes | no | yes |

A render fails, in this order of checks, with:

1. `invalid_size` unless 16 <= `S` <= 1024;
2. `invalid_frame` if the resolved style is not allowed for the shape or for the mode;
3. `low_contrast` if `ab` = 255 and the figure contrast of section 9 against `B` is below 200
   (that is, below 2:1);
4. `invalid_size` if the cell size `t` of section 7 is 0 (round shape with `double` or `thick`
   at `S` < 18).

An interface in which a fingerprint can be unset, or an option can hold a value outside its
type (an unknown shape or style, a colour or an alpha out of range), reports that before these
checks: `invalid_fingerprint` first, then `invalid_argument`.

## 7. Geometry

```
w = max(1, floor(S / 48))        frame line width
g = max(1, floor(S / 48))        gutter between cells

square shape:
    m = max(4 * w, floor(S / 12))
    t = floor((S - 2 * m - 3 * g) / 4)
round shape:
    k = 3 for the styles double and thick, otherwise 1
    m = k * w + g
    t = the largest integer >= 0 with 2 * (4 * t + 3 * g)^2 <= (S - 2 * m)^2

G = 4 * t + 3 * g                grid side
o = floor((S - G) / 2)           offset of the grid from the left and from the top
```

Cell `i` covers the pixels `x0 <= x < x0 + t`, `y0 <= y < y0 + t` with
`x0 = o + (i mod 4) * (t + g)` and `y0 = o + floor(i / 4) * (t + g)`. Pixel (0, 0) is the top left
pixel of the image. In the round shape the grid is inscribed in the circle of the image with the
margin `m`, so no cell is clipped.

## 8. Rasterisation

### 8.1 Samples

Every pixel is evaluated at 16 samples, a 4 x 4 grid. In sample coordinates the image spans
0..8S on both axes and pixel (x, y) has the samples

```
U = 2 * (4 * x + p) + 1,   V = 2 * (4 * y + q) + 1      for p, q in 0..3
```

All sample coordinates are odd, so no sample lies on an integer boundary. One pixel is 8 units.
With `W8 = 8 * w` and `S8 = 8 * S`, define for a sample

```
du = min(U, S8 - U)     dv = min(V, S8 - V)     e = min(du, dv)
dx = U - 4 * S          dy = V - 4 * S          d2 = dx^2 + dy^2        r = 4 * S
```

### 8.2 Outline

The outline is the part of the canvas that belongs to the image. Samples outside it are
transparent whatever the background is.

| Shape and style | A sample is inside the outline if |
|---|---|
| square, `rounded` | not (`du < R` and `dv < R`), or `(R - du)^2 + (R - dv)^2 <= R^2` |
| square, `chamfered` | `du + dv >= C` |
| square, any other style | always |
| round, any style | `d2 <= r^2` |

with

```
R = min(16 * o, 4 * S)                               corner radius, never beyond the centre lines
D = floor((W8 * 1414 + 500) / 1000)                  width of a diagonal line
C = max(8, 8 * floor((16 * o - D - W8) / 8))         chamfer cut
```

### 8.3 Frame

A sample inside the outline belongs to the frame if:

| Shape and style | Condition |
|---|---|
| `none` | never |
| square, `plain` | `e < W8` |
| square, `double` | `e < W8`, or `2 * W8 <= e < 3 * W8` |
| square, `thick` | `e < 3 * W8` |
| square, `brackets` | `e < W8` and `max(du, dv) < 8 * floor(S / 4)` |
| square, `chamfered` | `e < W8`, or `du + dv - C < D` |
| square, `rounded` | if `du < R` and `dv < R`: `(R - du)^2 + (R - dv)^2 > (R - W8)^2`; otherwise `e < W8` |
| round, `plain` | `d2 > (r - W8)^2` |
| round, `double` | `d2 > (r - W8)^2`, or `(r - 3 * W8)^2 < d2 <= (r - 2 * W8)^2` |
| round, `thick` | `d2 > (r - 3 * W8)^2` |
| round, `gaps` | `d2 > (r - W8)^2` and `abs(abs(dx) - abs(dy)) >= 8 * max(1, floor(S / 24))` |
| round, `ticks` | `d2 > (r - W8)^2`, or `abs(dx) < W8` and `abs(dy) >= Q`, or `abs(dy) < W8` and `abs(dx) >= Q` |

with, for `ticks`,

```
I = r - W8                                   inner radius of the ring
Q = I - max(8, floor((I - 4 * G) * 6 / 10))  where the ticks end
```

### 8.4 Figures

Inside a cell a pixel has the cell-local coordinates `px = x - x0`, `py = y - y0` and the
cell-local samples `u = 2 * (4 * px + p) + 1`, `v = 2 * (4 * py + q) + 1` for `p`, `q` in 0..3.
With `H = 4 * t`
(half the cell side in sample units) a sample belongs to the figure if:

| Figure | Condition |
|---|---|
| square | always |
| circle | `(u - H)^2 + (v - H)^2 <= H^2` |
| triangle up | `2 * abs(u - H) <= v` |
| triangle down | `2 * abs(u - H) <= 2 * H - v` |
| triangle right | `2 * abs(v - H) <= 2 * H - u` |
| triangle left | `2 * abs(v - H) <= u` |

A triangle is isosceles: its base is one full side of the cell and its apex is the midpoint of
the opposite side. "Pointing up" means the base is the bottom side.

### 8.5 Compositing

A pixel mixes `nf` samples of a foreground colour `F` with alpha `a` laid over the background,
`nb` samples of the background alone and `16 - nf - nb` transparent samples:

```
MIX(F, a, nf, nb):
    A     = nf * (255 * a + ab * (255 - a)) + nb * 255 * ab
    alpha = floor((A + 2040) / 4080)
    if alpha = 0: the pixel is 00 00 00 00
    for each channel c:
        P        = nf * (255 * a * F[c] + ab * (255 - a) * B[c]) + nb * 255 * ab * B[c]
        pixel[c] = floor((P + floor(A / 2)) / A)
    pixel alpha = alpha
```

The image is produced in two steps:

1. Every pixel (x, y) of the canvas: `nin` = the number of its samples inside the outline, `nfr` =
   the number of those that belong to the frame; the pixel is `MIX(808080, af, nfr, nin - nfr)`.
2. Every non-empty cell, every pixel of the cell: `nc` = the number of its samples that belong to
   the figure; the pixel is replaced by `MIX(colour of the cell, 255, nc, 16 - nc)`.

The geometry guarantees that every cell pixel lies wholly inside the outline and off the frame,
so step 2 never discards frame samples.

With the default background the formulas reduce to `floor((n * F + (16 - n) * 255 + 8) / 16)` per
channel with alpha 255; with `ab` = 0 to the figure colour with alpha `floor((n * 255 + 8) / 16)`.

## 9. Contrast

The palette has a contrast of at least 3:1 against white and against `121212`. Other backgrounds
can make figures vanish, which makes images of different inputs look alike, so an implementation
measures the contrast and refuses opaque backgrounds that are too close to a palette colour.

```
Y(c)      = 2126 * LIN[c.r] + 7152 * LIN[c.g] + 722 * LIN[c.b]
CR(c1,c2) = floor(100 * (max(Y(c1), Y(c2)) + 500000000) / (min(Y(c1), Y(c2)) + 500000000))
OVER(c, a, under)[ch] = floor((a * c[ch] + (255 - a) * under[ch] + 127) / 255)
```

`LIN` is the table of appendix A (the sRGB transfer function times 10^6). `CR` is the WCAG 2
contrast ratio times 100, rounded down; intermediate values need 64-bit integers.

For a background `B` with alpha `ab`, a frame alpha `af` and the colour `page` that the host
paints underneath:

```
E              = OVER(B, ab, page)                    what is seen behind the figures
figures_x100   = the minimum of CR(palette colour, E) over the four palette colours
frame_x100     = CR(OVER(808080, af, E), E)
```

A render with `ab` = 255 fails with `low_contrast` if `figures_x100 < 200`; `page` is irrelevant
then. For `ab` < 255 the render cannot know `page`, so the host SHOULD measure with its page
colour. Hosts SHOULD warn their user when `figures_x100 < 300`. `frame_x100` is informative.

## 10. Pixel formats

The canonical raster is `S * S` pixels, row-major from the top left, 4 bytes per pixel in the
order R, G, B, A with straight (non-premultiplied) alpha. The 32-bit form used by some platforms
is `A * 2^24 + R * 2^16 + G * 2^8 + B` per pixel, also straight alpha.

The encoders of sections 11 to 13 accept any image of 1..4096 by 1..4096 pixels in the canonical
format; other dimensions or a buffer of the wrong length are the error `invalid_image`.

Formats without alpha flatten every pixel over a matte colour `M` chosen by the caller (default
`FFFFFF`); with `pa` the alpha of the pixel,
`flat[ch] = floor((pa * pixel[ch] + (255 - pa) * M[ch] + 127) / 255)`.

## 11. PNG

```
89 50 4E 47 0D 0A 1A 0A
IHDR: width u32be, height u32be, bit depth 8, colour type T, compression 0, filter 0, interlace 0
sRGB: rendering intent 0
IDAT: one chunk with the whole zlib stream
IEND
```

- Every chunk is `u32be(length) || type || data || u32be(CRC-32 of type || data)`.
- `T` = 2 (truecolour, 3 bytes per pixel) if every pixel has alpha 255, otherwise 6 (truecolour
  with alpha, 4 bytes per pixel, straight alpha). `bpp` is 3 or 4 accordingly.
- The raw stream is, for each row from the top, the filter byte `00` followed by the pixels.
  `stride = 1 + width * bpp`.
- The zlib stream is `78 01`, one deflate block, and the Adler-32 of the raw stream as `u32be`.
- The deflate block (RFC 1951) has `BFINAL` = 1 and `BTYPE` = 01 (fixed Huffman codes) and ends
  with the end-of-block symbol. Unused bits of the last byte are 0.
- Matching is greedy over two candidate distances, in this order: `bpp` (the previous pixel) and
  `stride` (the pixel above):

```
i = 0
while i < n:                                  n = length of the raw stream
    best = 0
    for d in (bpp, stride):
        if i >= d:
            L = the largest value <= min(258, n - i) with raw[i + k] = raw[i - d + k] for all k < L
            if L > best: best = L, dist = d
    if best >= 3: emit the match (best, dist); i = i + best
    else:         emit the literal raw[i];     i = i + 1
```

A match may overlap the current position (`dist < best`), as deflate allows.

## 12. BMP

A BMP file is the flattened image (section 10) as 24-bit `BI_RGB`, bottom-up:

```
"BM" || u32le(file size) || u16le(0) || u16le(0) || u32le(54)
u32le(40) || u32le(width) || u32le(height) || u16le(1) || u16le(24) || u32le(0)
       || u32le(row * height) || u32le(2835) || u32le(2835) || u32le(0) || u32le(0)
rows from the bottom one up: for each pixel B, G, R; then 0..3 zero bytes to a multiple of 4
```

with `row = 4 * floor((3 * width + 3) / 4)` and `file size = 54 + row * height`.

## 13. JPEG

A JPEG file is the flattened image (section 10) as baseline sequential JFIF, 8 bits per sample,
three components without subsampling (4:4:4), with the example tables of ITU-T T.81 annex K.
The quality `q` MUST be in 50..100 (default 92), otherwise the error is `invalid_quality`
(`invalid_image` of section 10 is checked first).
JPEG is offered for compatibility; PNG or raw pixels are preferred, because JPEG rings on flat
colour edges.

### 13.1 File structure

```
FF D8
FF E0 00 10 "JFIF" 00 01 02 00 00 01 00 01 00 00
FF DB 00 43 00 <64 luminance quantiser values in zigzag order>
FF DB 00 43 01 <64 chrominance quantiser values in zigzag order>
FF C0 00 11 08 u16be(height) u16be(width) 03  01 11 00  02 11 01  03 11 01
FF C4 <length> 00 <DC luminance table>       FF C4 <length> 10 <AC luminance table>
FF C4 <length> 01 <DC chrominance table>     FF C4 <length> 11 <AC chrominance table>
FF DA 00 0C 03  01 00  02 11  03 11  00 3F 00
<entropy-coded data>
FF D9
```

A Huffman table segment holds the 16 code-length counts followed by the symbols (appendix B);
its length field is `u16be(2 + 1 + 16 + number of symbols)`. All multi-byte fields of a JPEG
file are big-endian.

### 13.2 Quantiser tables

With the annex K base tables of appendix B in natural (row-major) order:

```
scale = 200 - 2 * q
Qt[k] = clamp(floor((base[k] * scale + 50) / 100), 1, 255)        k = 0..63
```

### 13.3 Colour conversion

```
Y  = floor(( 19595 * R + 38470 * G +  7471 * B +   32768) / 65536)
Cb = floor((-11059 * R - 21709 * G + 32768 * B + 8421375) / 65536)
Cr = floor(( 32768 * R - 27439 * G -  5329 * B + 8421375) / 65536)
```

All three numerators are non-negative. The sample fed to the transform is the value minus 128.

### 13.4 Blocks

The image is cut into 8 x 8 blocks, row-major from the top left. A block that extends beyond
the image repeats the last column and the last row: the sample at (x, y) is taken at
(`min(x, width - 1)`, `min(y, height - 1)`). For each block position the three blocks are coded
in the order Y, Cb, Cr.

### 13.5 Forward transform

`DESCALE(x, n) = floor((x + 2^(n-1)) / 2^n)`; here `x` can be negative and the division is a true
floor. The transform works in place on the 64 samples `d[0..63]` of a block, first on each row,
then on each column, with the constants

```
F0298 =  2446   F0390 =  3196   F0541 =  4433   F0765 =  6270   F0899 =  7373   F1175 =  9633
F1501 = 12299   F1847 = 15137   F1961 = 16069   F2053 = 16819   F2562 = 20995   F3072 = 25172
```

One pass over eight values `d0..d7` (a row, or a column in the second pass):

```
t0 = d0 + d7   t7 = d0 - d7   t1 = d1 + d6   t6 = d1 - d6
t2 = d2 + d5   t5 = d2 - d5   t3 = d3 + d4   t4 = d3 - d4
t10 = t0 + t3   t13 = t0 - t3   t11 = t1 + t2   t12 = t1 - t2

first pass:   d0 = (t10 + t11) * 4               d4 = (t10 - t11) * 4               n = 11
second pass:  d0 = DESCALE(t10 + t11, 2)         d4 = DESCALE(t10 - t11, 2)         n = 15

z1 = (t12 + t13) * F0541
d2 = DESCALE(z1 + t13 * F0765, n)
d6 = DESCALE(z1 - t12 * F1847, n)

z1 = t4 + t7   z2 = t5 + t6   z3 = t4 + t6   z4 = t5 + t7   z5 = (z3 + z4) * F1175
t4 = t4 * F0298   t5 = t5 * F2053   t6 = t6 * F3072   t7 = t7 * F1501
z1 = -z1 * F0899   z2 = -z2 * F2562   z3 = -z3 * F1961 + z5   z4 = -z4 * F0390 + z5
d7 = DESCALE(t4 + z1 + z3, n)   d5 = DESCALE(t5 + z2 + z4, n)
d3 = DESCALE(t6 + z2 + z3, n)   d1 = DESCALE(t7 + z1 + z4, n)
```

All intermediate values fit a signed 32-bit integer. The result is the DCT scaled by 8.

### 13.6 Quantisation

For the coefficient `c = d[k]` and `dv = 8 * Qt[k]`:
`value = floor((c + dv / 2) / dv)` if `c >= 0`, otherwise `-floor((-c + dv / 2) / dv)`.
AC values (`k` > 0) are then clamped to -1023..1023, the range the baseline tables can code.
DC values lie in -1024..1016 by construction.

### 13.7 Entropy coding

Standard baseline Huffman coding (T.81 annex F) with the tables of appendix B, codes generated
as in T.81 annex C:

- The DC coefficient is coded as the difference to the previous block of the same component
  (initially 0): the category symbol, then the additional bits.
- The AC coefficients are coded in zigzag order (appendix B) as `run * 16 + category` symbols
  with additional bits, where `run` is the number of zeros before the coefficient. A run of 16
  or more is cut into `F0` symbols (16 zeros each), which are emitted only ahead of a non-zero
  coefficient; a block that ends with zeros ends with `00` (end of block) instead.
- The category of a value `v` is the number of bits of `abs(v)`; the additional bits are the low
  `category` bits of `v` if `v >= 0`, otherwise of `v - 1`.
- Bits are packed most significant bit first; the last byte is filled with 1 bits; every byte
  `FF` of the data, including a filled last byte, is followed by `00`.

## 14. Errors

| Code | Name | Meaning |
|---|---|---|
| 0 | `ok` | success |
| 1 | `empty_input` | the input has no bytes |
| 2 | `input_too_large` | the input is longer than 1 048 576 bytes |
| 3 | `invalid_hex` | the string is not hexadecimal as section 3 defines it |
| 4 | `invalid_key` | the key is not 32 bytes, is all zero or is unset |
| 5 | `invalid_digest` | the base digest is not 32 bytes or is unset |
| 6 | `invalid_fingerprint` | the fingerprint is not 32 bytes, has an unknown mode or is unset |
| 7 | `invalid_size` | the size is outside 16..1024 or leaves no room for the cells |
| 8 | `invalid_frame` | the frame style is not allowed for the shape or the mode |
| 9 | `low_contrast` | the opaque background is too close to a palette colour |
| 10 | `invalid_quality` | the JPEG quality is outside 50..100 |
| 11 | `invalid_image` | the image dimensions or its buffer length are invalid |
| 12 | `buffer_too_small` | the caller's buffer cannot hold the result |
| 13 | `out_of_memory` | an allocation failed |
| 14 | `invalid_argument` | a null pointer, a value outside its type (an unknown enumeration value, a colour or an alpha out of range) or ill-formed text |

Numeric codes are part of the C ABI. Languages with exceptions MAY map them to exceptions.

## 15. Golden vectors

`testdata/vectors.tsv` is UTF-8 text with LF line ends. Lines that start with `#` are
comments. Every other line is a record: fields separated by one tab, the first field is the
record type. Bytes are lowercase hexadecimal; `-` stands for an absent value.

Inputs are written as `hex:<bytes>` (possibly empty) or, for long inputs, `fill:<byte>:<count>`
(`count` copies of one byte); the input of a `text` record is the UTF-8 encoding of the text.
Options are written as the names of section 6; a background is `RRGGBBAA`.

Test images that are not renders are written as a pattern. `flat:<RRGGBBAA>` fills every pixel
with one value. `noise:<seed>` fills the RGBA buffer byte by byte from a generator:
`x = seed` at the start, then for every byte `x = (x * 1103515245 + 12345) mod 2^31` and the
byte is `floor(x / 2^16) mod 256`. `opaque:<seed>` is the same with every alpha byte then set
to `FF`.

| Type | Fields after the type |
|---|---|
| `D` | id, kind (`binary`, `text`), input, key or `-`, M1 or `-` if longer than 256 bytes, d0, s, M2, universal fp, keyed fp or `-`, KCV or `-`, universal cells, keyed cells or `-`, universal tag, keyed tag or `-` |
| `H` | id, the hexadecimal string as the hex of its UTF-8 bytes, the decoded bytes or the error name |
| `K` | id, key bytes, KCV or the error name |
| `C` | id, background, frame alpha, page `RRGGBB`, figures_x100, frame_x100 |
| `R` | id, fp, mode (`universal`, `keyed`), size, shape, frame, background, frame alpha, JPEG quality, matte `RRGGBB`, then the SHA-256 of the RGBA buffer, of the PNG, of the BMP and of the JPEG file |
| `E` | id, fp, mode, size, shape, frame, background, frame alpha, the error name |
| `G` | id, file name under `golden/`, fp, mode, size, shape, frame, background, frame alpha |
| `W` | id, fp, mode, shape, frame, background, frame alpha, first size, last size, then the SHA-256 of the RGBA buffers of every size from the first to the last, concatenated |
| `I` | id, width, height, pattern, JPEG quality, matte, then the SHA-256 of the RGBA buffer, of the PNG, of the BMP and of the JPEG file |
| `F` | id, operation, its arguments, the error name; the operations are `digest <kind> <input>`, `jpeg <quality>` (any valid image) and `image <width> <height> <buffer length>` (any encoder) |

Cells are written as 16 pairs of digits, the layout value of the figure and the colour index.
A `G` record names a PNG file that MUST equal, byte for byte, the PNG encoding of that render.
An implementation conforms if it reproduces every field of every record.

## Appendix A. `LIN`, the sRGB transfer function times 10^6

`LIN[v] = round(10^6 * f(v / 255))` with `f(c) = c / 12.92` for `c <= 0.04045` and
`((c + 0.055) / 1.055)^2.4` otherwise. The table is normative; the formula explains it.

```
      0,     304,     607,     911,    1214,    1518,    1821,    2125,
   2428,    2732,    3035,    3347,    3677,    4025,    4391,    4777,
   5182,    5605,    6049,    6512,    6995,    7499,    8023,    8568,
   9134,    9721,   10330,   10960,   11612,   12286,   12983,   13702,
  14444,   15209,   15996,   16807,   17642,   18500,   19382,   20289,
  21219,   22174,   23153,   24158,   25187,   26241,   27321,   28426,
  29557,   30713,   31896,   33105,   34340,   35601,   36889,   38204,
  39546,   40915,   42311,   43735,   45186,   46665,   48172,   49707,
  51269,   52861,   54480,   56128,   57805,   59511,   61246,   63010,
  64803,   66626,   68478,   70360,   72272,   74214,   76185,   78187,
  80220,   82283,   84376,   86500,   88656,   90842,   93059,   95307,
  97587,   99899,  102242,  104616,  107023,  109462,  111932,  114435,
 116971,  119538,  122139,  124772,  127438,  130136,  132868,  135633,
 138432,  141263,  144128,  147027,  149960,  152926,  155926,  158961,
 162029,  165132,  168269,  171441,  174647,  177888,  181164,  184475,
 187821,  191202,  194618,  198069,  201556,  205079,  208637,  212231,
 215861,  219526,  223228,  226966,  230740,  234551,  238398,  242281,
 246201,  250158,  254152,  258183,  262251,  266356,  270498,  274677,
 278894,  283149,  287441,  291771,  296138,  300544,  304987,  309469,
 313989,  318547,  323143,  327778,  332452,  337164,  341914,  346704,
 351533,  356400,  361307,  366253,  371238,  376262,  381326,  386429,
 391572,  396755,  401978,  407240,  412543,  417885,  423268,  428690,
 434154,  439657,  445201,  450786,  456411,  462077,  467784,  473531,
 479320,  485150,  491021,  496933,  502886,  508881,  514918,  520996,
 527115,  533276,  539479,  545724,  552011,  558340,  564712,  571125,
 577580,  584078,  590619,  597202,  603827,  610496,  617207,  623960,
 630757,  637597,  644480,  651406,  658375,  665387,  672443,  679542,
 686685,  693872,  701102,  708376,  715694,  723055,  730461,  737910,
 745404,  752942,  760525,  768151,  775822,  783538,  791298,  799103,
 806952,  814847,  822786,  830770,  838799,  846873,  854993,  863157,
 871367,  879622,  887923,  896269,  904661,  913099,  921582,  930111,
 938686,  947307,  955973,  964686,  973445,  982251,  991102, 1000000
```

## Appendix B. JPEG tables

Zigzag order (`zigzag[i]` is the natural index of the i-th coefficient):

```
 0  1  8 16  9  2  3 10    17 24 32 25 18 11  4  5
12 19 26 33 40 48 41 34    27 20 13  6  7 14 21 28
35 42 49 56 57 50 43 36    29 22 15 23 30 37 44 51
58 59 52 45 38 31 39 46    53 60 61 54 47 55 62 63
```

Base quantiser tables in natural order, luminance then chrominance:

```
16 11 10 16  24  40  51  61        17 18 24 47 99 99 99 99
12 12 14 19  26  58  60  55        18 21 26 66 99 99 99 99
14 13 16 24  40  57  69  56        24 26 56 99 99 99 99 99
14 17 22 29  51  87  80  62        47 66 99 99 99 99 99 99
18 22 37 56  68 109 103  77        99 99 99 99 99 99 99 99
24 35 55 64  81 104 113  92        99 99 99 99 99 99 99 99
49 64 78 87 103 121 120 101        99 99 99 99 99 99 99 99
72 92 95 98 112 100 103  99        99 99 99 99 99 99 99 99
```

Huffman tables: the number of codes of each length 1..16, then the symbols in code order.

```
DC luminance    00 01 05 01 01 01 01 01 01 00 00 00 00 00 00 00
                00 01 02 03 04 05 06 07 08 09 0A 0B
DC chrominance  00 03 01 01 01 01 01 01 01 01 01 00 00 00 00 00
                00 01 02 03 04 05 06 07 08 09 0A 0B
AC luminance    00 02 01 03 03 02 04 03 05 05 04 04 00 00 01 7D
                01 02 03 00 04 11 05 12 21 31 41 06 13 51 61 07 22 71 14 32 81 91 A1 08
                23 42 B1 C1 15 52 D1 F0 24 33 62 72 82 09 0A 16 17 18 19 1A 25 26 27 28
                29 2A 34 35 36 37 38 39 3A 43 44 45 46 47 48 49 4A 53 54 55 56 57 58 59
                5A 63 64 65 66 67 68 69 6A 73 74 75 76 77 78 79 7A 83 84 85 86 87 88 89
                8A 92 93 94 95 96 97 98 99 9A A2 A3 A4 A5 A6 A7 A8 A9 AA B2 B3 B4 B5 B6
                B7 B8 B9 BA C2 C3 C4 C5 C6 C7 C8 C9 CA D2 D3 D4 D5 D6 D7 D8 D9 DA E1 E2
                E3 E4 E5 E6 E7 E8 E9 EA F1 F2 F3 F4 F5 F6 F7 F8 F9 FA
AC chrominance  00 02 01 02 04 04 03 04 07 05 04 04 00 01 02 77
                00 01 02 03 11 04 05 21 31 06 12 41 51 07 61 71 13 22 32 81 08 14 42 91
                A1 B1 C1 09 23 33 52 F0 15 62 72 D1 0A 16 24 34 E1 25 F1 17 18 19 1A 26
                27 28 29 2A 35 36 37 38 39 3A 43 44 45 46 47 48 49 4A 53 54 55 56 57 58
                59 5A 63 64 65 66 67 68 69 6A 73 74 75 76 77 78 79 7A 82 83 84 85 86 87
                88 89 8A 92 93 94 95 96 97 98 99 9A A2 A3 A4 A5 A6 A7 A8 A9 AA B2 B3 B4
                B5 B6 B7 B8 B9 BA C2 C3 C4 C5 C6 C7 C8 C9 CA D2 D3 D4 D5 D6 D7 D8 D9 DA
                E2 E3 E4 E5 E6 E7 E8 E9 EA F2 F3 F4 F5 F6 F7 F8 F9 FA
```

## Appendix C. References

FIPS 180-4 (SHA-256); RFC 2104 (HMAC); RFC 8018 (PBKDF2); RFC 1950, RFC 1951 (zlib, deflate);
the PNG specification (W3C); ITU-T T.81 (JPEG); JFIF 1.02; WCAG 2.2 (contrast ratio);
IEC 61966-2-1 (sRGB).
