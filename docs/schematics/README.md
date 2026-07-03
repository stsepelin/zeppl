# Schematics

Source of truth for the analog/wiring drawings. Each `.py` is a
[schemdraw](https://schemdraw.readthedocs.io/) script that renders the
`.svg` next to it; the SVGs are committed so the docs render on GitHub
without any toolchain.

| Drawing | Built in | What it is |
|---|---|---|
| `j1850_rx.py/.svg` | Phase 3 Stage 1–2 | J1850 RX front end: 7.5V zener clamp + 10k/4.7k divider → sniffer GPIO. Build this alone first — it physically cannot disturb the bus. |
| `j1850_tx.py/.svg` | Phase 3 Stage 4 | TX high-side stage: IRLZ44N low-side switch driving a PNP that sources +12V through 100Ω onto the bus (zener in the RX drawing sets the ~7V dominant level). R6 base pull-up keeps the PNP hard off when idle. |
| `discrete_divider.py/.svg` | Phase 6 | 12V discrete-signal divider (10k/2.7k + optional 3.3V zener), ×6 for turns/beam/neutral/oil/ignition. Sized for 14.4V charging voltage. |

## Regenerate

```sh
python3 -m venv .venv && .venv/bin/pip install schemdraw
for f in *.py; do .venv/bin/python "$f"; done
```

Edit the `.py`, re-run, commit both files. Don't hand-edit the SVGs.

## Conventions

- Component designators (R1, Q2, D1…) are stable across drawings and
  match the prose in `../00-MASTER-PROJECT-PLAN.md`.
- Values that came out of analysis carry their reasoning as a caption
  inside the drawing (e.g. the 14.4V charging-voltage math), so a
  printout taken to the bench is self-contained.
