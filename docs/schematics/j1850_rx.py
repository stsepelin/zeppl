# J1850 RX front end (Phase 3 Stage 1-2, the sniff build).
# Regenerate: see README.md in this directory.
import schemdraw
import schemdraw.elements as elm

with schemdraw.Drawing(file="j1850_rx.svg", show=False) as d:
    d.config(unit=2.5, fontsize=11)

    bus = d.add(elm.Dot(open=True).label("J1850 BUS\nharness pin 7 (LGN/V)", loc="left"))
    d.add(elm.Line().right().length(1.5))
    tap = d.add(elm.Dot())

    # Clamp: sets the ~7V dominant level and eats transients. Cathode
    # faces the bus — it works in reverse breakdown; forward would pin
    # the bus at 0.7V.
    d.add(elm.Zener().down().reverse().at(tap.center).label("D1\n7.5V 1W", loc="bottom"))
    d.add(elm.Ground())

    # Divider: 7V active -> ~2.2V at the GPIO, inside the P4's 3.3V world.
    d.add(elm.Resistor().right().at(tap.center).label("R1\n10k"))
    node = d.add(elm.Dot())
    d.add(elm.Resistor().down().at(node.center).label("R2\n4.7k", loc="bottom"))
    d.add(elm.Ground())

    d.add(elm.Line().right().at(node.center).length(1.5))
    d.add(elm.Dot(open=True).label("ESP32-P4 GPIO 20\n(VROD_J1850_RX_GPIO)", loc="right"))

    d.add(
        elm.Label()
        .at((3.5, -4.6))
        .label("Bench check before the bike: 7V on BUS node -> ~2.2V at GPIO node;\n"
               "12V injected on BUS node -> clamps at ~7.5V. Grounds common with the P4.",
               fontsize=9)
    )
