# Security

What hh protects against, what it does not, and how to report a problem.

## The attacks

- **Address poisoning.** An attacker grinds an address whose first and last characters match an
  address the victim uses and plants it in the victim's history with a dust, zero-value or
  counterfeit-token transfer. The victim later copies the wrong entry. Measured on Ethereum and
  BSC over two years: 270 million poisoning transfers, 17 million victims, 83.8 million USD stolen;
  most lookalikes match up to 14 hex digits, the strongest group 20 (Tsuchiya et al., USENIX
  Security 2025).
- **Clipboard or in-memory substitution.** Malware replaces a copied address with a lookalike
  mined in advance.

Both work because people compare only the ends of a long string. A picture derived from the
whole input changes completely when any character changes.

## What an attacker can afford

One current GPU tries about 1.4 billion addresses per second. Identicons seeded with a few bytes
of the address, or built on a non-cryptographic generator, fall in seconds to minutes. hh makes
the forger pay in three ways:

1. **Every cell must match.** The picture has 16 cells of 2.75 bits of figure and, when filled,
   2 bits of colour: 68 bits nominally, 44 of them in the figure channel, which does not depend on
   colour vision. A forgery that may differ from the target in two cells still costs about 2^52
   tries. A search of 2^24 candidates finds pictures that share the pattern of empty cells and
   still differ in two to four colours and three to six figures (`docs/design`).
2. **Every try is slow.** The base digest is 16 384 iterations of PBKDF2-HMAC-SHA-256: about 11
   bits of extra work per try, a few milliseconds for a host that caches it per address.
3. **Keyed mode cannot be ground at all.** The fingerprint is an HMAC-SHA-256 of the base digest
   under a 32-byte secret of the wallet. Without the key nobody can compute the picture the victim
   will see, so a lookalike address gets an unrelated random picture. Poisoning and clipboard
   substitution target one wallet at a time, which is exactly the case keyed mode covers.

## What a picture proves

- People reliably tell apart about 30 bits of a visual hash, fewer from memory, and they compare
  the overall gist first. **A picture that looks the same is strong evidence, not proof**; a
  picture that looks different is proof of a different input.
- hh therefore complements the text check and never replaces it. For a check that is certain,
  compare the six-character tag (`K7Q-M2X`) or the full address. Hosts must say so in their copy.
- A decision should be backed by a picture of at least 64 dp, better 96, shown beside the picture
  it is compared with. At 4 x 4 the outer ring of cells cannot be taken in with one glance;
  comparison is a short scan.
- In universal mode a mass attacker who grinds against many victims at once divides the cost per
  victim by the number of targets. Inside a wallet use keyed mode.

## The key

- Exactly 32 bytes, uniformly random or the output of a key derivation function; the all-zero key
  is rejected, because a zero-filled buffer is what a failed key load looks like and it must never
  produce valid pictures. There is no passphrase form.
- Recommended source: a key derived from the wallet seed on a dedicated path (for example
  SLIP-0021 with the labels `"HumanizedHash"`, `"keyed"`, `"0"`), so that it needs no storage and
  survives a restore. A mistyped passphrase then shows up as unfamiliar pictures.
- Pictures do not leak the key: HMAC outputs on known inputs reveal neither the key nor the output
  for another input. A leaked screenshot lets an attacker replay that one picture for that one
  address and gives nothing towards a lookalike.
- Because stretching precedes keying, keyed mode is never weaker than universal mode, even if the
  key leaks.
- The key check value (4 bytes) lets a host notice that the key, and with it every picture,
  changed. A host must explain such a change in a blocking notice, as messengers do for a changed
  safety number. Rotation is an explicit user ceremony, never silent.
- The library wipes key material from its own buffers. Keeping the key out of logs, crash dumps
  and swap is the host's task.

## The two modes must not be confused

Comparing a keyed picture with a universal one always fails, which a user reads as "the address
was replaced". Hosts keep the modes apart:

- one picture per address per screen, with a caption that names the mode in words;
- keyed pictures carry a frame marker by default in the square shape; exported or shared pictures
  are always universal;
- mode confusion can only produce a false alarm, never a false match: the two fingerprints of one
  input are unrelated.

## Backgrounds

The palette keeps a contrast of 3:1 against white and against `121212`. A figure that vanishes
into the background reads as an empty cell and makes pictures of different inputs alike, so the
library refuses opaque backgrounds with less than 2:1 against any palette colour and reports the
contrast for any other. Hosts that let users choose a background should warn below 3:1.

## Out of scope

- Malware that can read the key, drive the user interface or alter what the screen shows.
- An attacker who controls the application that computes the picture.
- Users who do not look. A picture that is never compared protects nobody; show it where the
  decision is made.

## Implementation notes

- Only standard constructions: SHA-256 (FIPS 180-4), HMAC (RFC 2104), PBKDF2 (RFC 8018), with
  domain-separated, length-prefixed inputs. Nothing cryptographic is invented here.
- No secret-dependent branches or table lookups in the keyed path beyond those of SHA-256 itself,
  which has none.
- Every entry point is total and bounds-checked; the test suite runs under AddressSanitizer and
  UndefinedBehaviorSanitizer, with deterministic pseudo-random loops over the entry points.

## Reporting a vulnerability

Please write to dm@censync.com with a description and, if possible, a reproduction. Do not open a
public issue for a suspected vulnerability.

## References

- Tsuchiya, Dong, Soska, Christin. Blockchain Address Poisoning. USENIX Security 2025.
- Hsiao et al. A Study of User-Friendly Hash Comparison Schemes. ACSAC 2009.
- Tan et al. Can Unicorns Help Users Compare Crypto Key Fingerprints? CHI 2017.
- Olembo et al. Developing and Testing a Visual Hash Scheme. 2013.
- Azimpourkivi, Topkara, Carbunar. Human Distinguishable Visual Key Fingerprints. USENIX Security 2020.
- Bellare. New Proofs for NMAC and HMAC. 2006. NIST SP 800-132. SLIP-0021.
