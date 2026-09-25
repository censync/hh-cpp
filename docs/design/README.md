# Design record

The sheets and tables the design of hh was decided on, kept as the record of why the frozen
constants are what they are. Everything here was produced by the design lab (`tools/lab`,
`hh_lab all`), which draws through the library's own rasteriser: a sheet shows exactly what hh
renders. Nothing in this directory is normative; `docs/SPEC.md` is.

## Decisions

| Topic | Decision | Evidence |
|---|---|---|
| Matrix and figures | 4 x 4 cells; one solid figure or nothing per cell: square, circle, triangle in four directions | `contact_*`, `directions_*` |
| Palette | `7A96C5`, `890AF0`, `C10445`, `D48200`: picked by eye from the candidate sheets, then tuned within 12 CAM02-UCS units to pass the gate | `palette_picked.png`, `palette_gate.tsv` |
| Triangle directions | four: even if left and right were never told apart, a cell keeps 2.5 bits of figure against 2.25 with two directions | `directions_64px_*`, `directions_key.tsv` |
| Shape | square by default; round as an option, the grid inscribed in the circle, no cell clipped | `markers_round_sizes.png`, `round_geometry.tsv` |
| Mode marker | universal pictures have no frame by default; keyed pictures carry a marker, rounded corners by default for the square; the round shape has none by default; since 1.1.0 every style is open to both modes and the host decides which style marks which mode | `markers_sizes.png`, `markers_list_*`, `markers_round_*` |
| Background | the host may set its colour and transparency and the transparency of the frame; opaque backgrounds below 2:1 against a palette colour are refused | `surfaces.png`, `surfaces.tsv` |
| Stretching | 16 384 PBKDF2 iterations | `bench/` |

## Palette gate

The method follows Petroff, "Accessible Color Sequences for Data Visualization" (2021): the
minimum pairwise distance in CAM02-UCS under normal vision, under protanomaly and deuteranomaly
at severities 10 % to 100 % (Machado, Oliveira and Fernandes 2009) and under tritanopia (Brettel,
Vienot and Mollon 1997). The gate for four colours is 20, with a WCAG contrast of at least 3:1
against white and against `121212`. The chosen palette scores 24.1; its weakest pair is red and
orange under full deuteranopia. Contrast against both white and a dark surface confines every
colour to a narrow band of luminance, so in greyscale the picture is carried by its shapes alone;
`contact_48px_white_grey.png` shows it.

## Lookalike grinding

`grind_*.png` show the nearest of 2^24 candidates for six targets, without stretching. hh is
searched in the order a forger would work (the pattern of empty cells, then colours, then figure
families, then directions): the best candidates match the empty cells and still differ in two to
four colours and three to six figures. The same search against icons in the style of Blockies and
Jazzicon is shown for comparison; `grinding.tsv` has the numbers.

## Reading the sheets

- `contact_<size>px_<white|dark>_<vision>.png`: 200 fingerprints (`SHA-256("hh-lab/contact/<i>")`)
  at one size; `vision` is normal, grey, or a simulated colour vision deficiency.
- `markers_*`: the frame styles on one fingerprint, and lists where universal (`u`) and keyed (`k`)
  pictures alternate as they would in an application.
- `surfaces.png`: host-chosen backgrounds with the lowest WCAG contrast of a palette colour.
- `directions_64px_blind.png` with `directions_key.tsv`: pairs that differ in one triangle
  direction, in the figure kind, or not at all.
- `bench/`: the cost of the base digest for 2^12 to 2^16 iterations, one thread. At the chosen
  2^14: 6.7 to 7.9 ms in C++ on a desktop core (Ryzen 7 5800H), 8.4 ms in Kotlin on its JVM; on a
  Pixel 5a 12 ms (big core) to 33 ms (little core) in C++ and 20 ms to 70 ms in Kotlin compiled
  ahead of time. Every run prints the base digest of twenty zero bytes, which is the golden
  vector `evm-zero`.
