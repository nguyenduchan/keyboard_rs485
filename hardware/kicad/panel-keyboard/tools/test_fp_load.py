import pcbnew  # type: ignore

path = r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\share\kicad\footprints\Resistor_SMD.pretty"
name = "R_0805_2012Metric"
fp = pcbnew.FootprintLoad(path, name)
print("loaded", fp)
