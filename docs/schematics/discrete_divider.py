# 12V discrete-signal divider (x6: turn L/R, high beam, neutral, oil
# pressure, ignition sense) — Phase 6. Sized for the real electrical
# system: "12V" is ~14.4V with the engine running; 10k/2.7k gives 3.06V
# at 14.4V and 2.55V at 12V. The optional 3.3V zener is belt-and-braces.
# Regenerate: see README.md in this directory.
import schemdraw
import schemdraw.elements as elm

with schemdraw.Drawing(file="discrete_divider.svg", show=False) as d:
    d.config(unit=2.5, fontsize=11)

    d.add(elm.Dot(open=True).label("12V discrete signal\n(harness pins 2/3/4/6/9/10)", loc="left"))
    d.add(elm.Line().right().length(1))
    d.add(elm.Resistor().right().label("R1\n10k"))
    node = d.add(elm.Dot())
    d.add(elm.Resistor().down().at(node.center).label("R2\n2.7k", loc="bottom"))
    d.add(elm.Ground())

    # Optional clamp in parallel with R2 (cathode up).
    d.add(elm.Line().right().at(node.center).length(1.2))
    z = d.add(elm.Zener().down().reverse().label("D1  3.3V\n(optional)", loc="bottom"))
    d.add(elm.Ground())

    d.add(elm.Line().up().at(node.center).length(1))
    d.add(elm.Line().right().length(2.6))
    d.add(elm.Dot(open=True).label("P4 GPIO input", loc="right"))

    d.add(
        elm.Label()
        .at((3.2, -4.4))
        .label("14.4V charging -> 3.06V at the GPIO; 12V -> 2.55V; 9V cranking dip -> 1.91V (reads low).\n"
               "The old 10k/4.7k pair put 4.6V on a 3.6V-abs-max pin at charging voltage - do not build it.",
               fontsize=9)
    )
