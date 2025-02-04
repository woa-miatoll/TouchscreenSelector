## ACPI Sample code
```
Device(TSSD)
{
    Name (_HID, "TSSD0000")

    Method (TSCF, 0, NotSerialized)
    {
        Name (CFG0, Package()
        {
            2,  // Number of touchscreens
            Package()
            {
                0x8756, // Panel ID
                "TSSD\\FTTS8756" // Touchscreen HardwareID
            },
            Package()
            {
                0x3675, // Panel ID
                "TSSD\\NTTS3675" // Touchscreen HardwareID
            }
        })
        Return (CFG0)
    }

    Device (TSC1)
    {
        Name (_ADR, 0)
        Name (_DEP, Package (0x03)
        {
            \_SB.PEP0,
            \_SB.GIO0,
            \_SB.SP12
        })

        Method (_CRS, 0, NotSerialized)
        {
            Name (RBUF, ResourceTemplate ()
            {
                SPISerialBus(0,,, 8,, 8000000, ClockPolarityLow, ClockPhaseFirst, "\\_SB.SP12",,,,)
                GpioInt(Edge, ActiveLow, ExclusiveAndWake, PullUp, 0, "\\_SB.GIO0", 0 , ResourceConsumer, , ) {9}
            })
            Return (RBUF)
        }
    }
}
```