# NEO-6M GPS module (GY-NEO6MV2) -> ESP32-P4 UART, receive-only.
# Map position source; no level shifting needed (module TX is 3.3V TTL,
# straight into a 3.3V P4 GPIO). Regenerate: see README.md in this directory.
import schemdraw
import schemdraw.elements as elm

with schemdraw.Drawing(file="gps_module.svg", show=False) as d:
    d.config(unit=2.5, fontsize=11)

    gps = d.add(
        elm.Ic(
            pins=[
                elm.IcPin(name="VCC", side="left", slot="3/3"),
                elm.IcPin(name="GND", side="left", slot="1/3"),
                elm.IcPin(name="TX", side="right", slot="3/3"),
                elm.IcPin(name="RX", side="right", slot="1/3"),
            ],
            edgepadW=1.4,
            label="GY-NEO6MV2\n(NEO-6M)\n+ patch antenna",
        )
    )

    # Power: 5V rail -> onboard MIC5205 makes the chip's 3.3V. ~45 mA.
    d.add(elm.Line().left().length(1.6).at(gps.VCC))
    d.add(elm.Vdd().label("+5V\n(bike-power 5V rail)"))
    d.add(elm.Line().left().length(1.6).at(gps.GND))
    d.add(elm.Ground().label("GND\n(common with P4)", loc="bottom"))

    # Data: module TX -> P4 RX (GPIO 21). RX-only gives fixes; the module's
    # 3.3V logic drives the 3.3V GPIO directly, no divider.
    d.add(elm.Line().right().length(2.0).at(gps.TX))
    d.add(elm.Dot(open=True).label("ESP32-P4 GPIO 21\n(VROD_GPS_RX_GPIO)", loc="right"))

    # RX pin optional: only needed to push UBX config (fix rate, constellations).
    d.add(elm.Line().right().length(2.0).at(gps.RX).linestyle("--").color("#888"))
    d.add(
        elm.Dot(open=True)
        .color("#888")
        .label("(optional) P4 TX\nVROD_GPS_TX_GPIO = -1", loc="right", color="#888")
    )

    d.add(
        elm.Label()
        .at((1.5, -4.4))
        .label(
            "9600 baud, NMEA RMC. RX-only: TX pin left unwired (fixes need RX alone).\n"
            "3.3V TTL straight to the GPIO -- no level shift. Antenna needs sky view.",
            fontsize=9,
        )
    )
