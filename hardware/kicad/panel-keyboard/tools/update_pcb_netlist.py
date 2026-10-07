import pcbnew  # type: ignore

ROOT = r"d:\Project\embedded\keyboard\hardware\kicad\panel-keyboard"
board = pcbnew.LoadBoard(f"{ROOT}/panel-keyboard.kicad_pcb")
netlist_path = f"{ROOT}/panel-keyboard.net"

reader = pcbnew.NETLIST_READER(board, netlist_path, True)
netlist = pcbnew.NETLIST(board, reader)

# Load footprints + nets from netlist (same as Update PCB from Schematic)
if hasattr(netlist, "ReplaceFootprints"):
    netlist.ReplaceFootprints()
if hasattr(netlist, "ReadNetlist"):
    netlist.ReadNetlist()

pcbnew.SaveBoard(f"{ROOT}/panel-keyboard.kicad_pcb", board)
print("PCB updated from netlist")
