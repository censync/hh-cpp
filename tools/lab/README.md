# hh design lab

Research tooling behind the design decisions: it draws the sheets of `docs/design/` and produces
the numbers behind the palette, the mode marker, the triangle directions and the stretching
constant. Its sheets are drawn by the library's own rasteriser (`src/raster.hpp`), so they show
exactly what hh renders; the lab adds only what the library fixes, such as other palettes. It is
built only with `-DHH_BUILD_LAB=ON` (preset `lab`), is never installed, and the library never
depends on it. Floating point and threads are allowed here and nowhere else in hh-cpp. Like the
rest of the repository it uses no third-party code: the colour science, the PNG writer and the
bitmap font are written here.

```sh
cmake --preset lab
cmake --build --preset lab
./build/lab/tools/lab/hh_lab all --out lab-out
```

`--quick` runs smaller workloads for a smoke test; `--threads N` limits the thread pool. Every
command is deterministic: the same build gives the same files.

## Commands

| Command | Output |
|---|---|
| `selftest` | Checks the colour science and the prior-art ports against reference values |
| `palette` | `palette/palette_gate.tsv`, `palette_conditions.tsv`, `palette_candidates.png`, `palette_picked.png` |
| `sheets` | `contact/contact_<size>px_<white\|dark>_<vision>.png`, `figure_statistics.tsv` |
| `markers` | `markers/markers_sizes.png`, `markers_list_<size>px_<white\|dark>.png`, `markers_round_sizes.png`, `markers_round_list_<size>px_<white\|dark>.png`, `round_contact_<size>px_<white\|dark>.png`, `round_geometry.tsv` |
| `surfaces` | `surfaces/surfaces.png`, `surfaces.tsv`: host-chosen background colours, alpha and frame colours |
| `grind` | `grinding/grind_hh_salience.png`, `grind_hh_gist.png`, `grind_blockies.png`, `grind_jazzicon.png`, `grinding.tsv` |
| `directions` | `directions/directions_<size>px_<labelled\|blind>.png`, `directions_key.tsv` |
| `bench` | `bench/stretch_cpp.tsv` |

## Method notes

- Renderer (`candidate.cpp`): an adapter onto the library's rasteriser (SPEC.md sections 6 to
  8), with a palette and cells of the lab's choosing. `selftest` checks that it equals
  `hh::render` pixel for pixel. The mode and contrast rules of `hh::render` do not apply in the
  lab, which also shows what the library refuses.
- Palette gate (`palette_gate.cpp`), after Petroff 2021: minimum pairwise CAM02-UCS distance
  under normal vision, Machado 2009 protan and deutan at severities 0.1 to 1.0 and Brettel 1997
  tritan; CIEDE2000 and OKLab reported alongside; WCAG contrast against white and `#121212`.
  Gate: 20 and 3:1. The search is simulated annealing over 8-bit sRGB with fixed seeds; tuned
  variants of a given palette stay within a CAM02-UCS radius or keep each hue within 10 degrees.
- Colour science (`colour.cpp`): CIECAM02 with the sRGB viewing conditions colorspacious uses
  (D65, `Y_b = 20`, `L_A = 64/pi/5`, average surround); Machado matrices from the authors'
  supplementary table; Brettel planes as precomputed by libDaltonLens. `selftest` checks CIE
  159:2004, colorspacious, Sharma 2005 and Ottosson reference values.
- Grinding (`grinding.cpp`): 2^24 candidates per scheme without stretching. hh is ordered by
  salience (empty-cell pattern, then colours, then figure families, then directions); all three
  schemes are also ordered by a pixel gist (mean OKLab distance on a 24 x 24 grid).
- Prior art (`prior_art.cpp`): Blockies and Jazzicon ports that follow the published JavaScript;
  `selftest` compares the Blockies port with the original code run in node and the Mersenne
  Twister with `std::mt19937`.
- Stretching benchmark (`stretch_bench.cpp`): the base digest derivation on one thread,
  with a check value that the hh-kotlin benchmark reproduces.
