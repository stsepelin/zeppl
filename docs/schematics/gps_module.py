# NEO-6M GPS module (GY-NEO6MV2) -> ESP32-P4 UART, receive-only.
# Map position source; no level shifting needed (module TX is 3.3V TTL,
# straight into a 3.3V P4 GPIO). Regenerate: see README.md in this directory.
import schemdraw
import schemdraw.elements as elm

with schemdraw.Drawing(file="gps_module.svg", show=False) as d:
    d.config(unit=2.0, fontsize=10)

    gps = d.add(
        elm.Ic(
            pins=[
                elm.IcPin(name="VCC", side="left", slot="2/2"),
                elm.IcPin(name="GND", side="left", slot="1/2"),
                elm.IcPin(name="TX", side="right", slot="2/2"),
                elm.IcPin(name="RX", side="right", slot="1/2"),
            ],
            size=(3.2, 2.6),
            label="GY-NEO6MV2\nNEO-6M\n+ ceramic\npatch antenna",
        ).anchor("VCC")
    )

    # ===== power: board 5V (40-pin header) -> onboard MIC5205 -> 3.3V, ~45 mA.
    # VCC_5V is USB-fed on the bench, bike-power-fed on the bike; same rail. =====
    d.add(elm.Line().left().at(gps.VCC).length(1.6))
    d.add(elm.Vdd().label("+5V\n(board VCC_5V,\n40-pin header)"))
    d.add(elm.Line().left().at(gps.GND).length(1.6))
    d.add(elm.Ground().label("GND\n(common w/ P4)", loc="bottom"))

    # ===== data: module TX -> P4 RX (GPIO 21), 3.3V TTL direct, no divider =====
    d.add(elm.Line().right().at(gps.TX).length(2.2))
    d.add(elm.Dot(open=True).label("ESP32-P4 GPIO 21\n(VROD_GPS_RX_GPIO)", loc="right"))

    # RX pin optional: only to push UBX config (fix rate / constellations).
    d.add(elm.Line().right().at(gps.RX).length(2.2).linestyle("--").color("#888"))
    d.add(
        elm.Dot(open=True)
        .color("#888")
        .label("(optional) P4 TX\nVROD_GPS_TX_GPIO = -1", loc="right", color="#888")
    )

    d.add(
        elm.Label()
        .at((1.6, -3.4))
        .label(
            "9600 baud, NMEA RMC. RX-only: TX pin left unwired (fixes need RX alone).\n"
            "3.3V TTL straight to the GPIO -- no level shift. Antenna needs sky view.",
            fontsize=9,
        )
    )
