"""LCSC / JLCPCB part numbers for every SMT part assembled by JLCPCB (rev C).

Choice rule (lô 20 bo): Basic/Preferred when an equivalent exists; Extended only when there is no
default-library part or it is clearly better (each Extended part costs $3 per order ≈ $0.15/board at 20 pcs).
THT parts (displays, keys, LEDs, buzzer, terminal blocks) are hand-soldered and not listed here.
"""

# ref -> (LCSC code, comment shown in the JLC BOM)
PARTS = {
    # ICs
    "U1": ("C529330", "STM32G030F6P6TR"),          # Extended: G030 TSSOP-20; no default-library MCU in this package
    "U6": ("C19187", "TM1638"),                    # Extended: no LED/key driver in the default library
    "U3": ("C8963", "SP3485EN-L/TR"),              # Basic
    "U4": ("C61063", "XL1509-5.0E1"),              # Basic
    "U5": ("C6186", "AMS1117-3.3"),                # Basic
    # discretes
    "D8": ("C8678", "SS34"),                       # Basic
    "DREV": ("C8678", "SS34"),                     # Basic
    "TVS1": ("C19077546", "SMAJ28CA"),             # Preferred (bidirectional 28 V stand-off)
    "TVS2": ("C32677", "PSM712-LF-T7"),            # Basic, 34 A surge (better than generic SM712)
    "QBZ": ("C8545", "2N7002"),                    # Basic
    "F1": ("C70112", "mSMD050-33V 0.5A"),          # Extended: no PPTC in the default library
    "L1": ("C36414", "SWPA6045S470MT 47uH"),       # Extended: no power inductor in the default library
    # ferrite beads (Basic)
    "FBIN": ("C1017", "600R@100MHz 0805"),
    "FB1": ("C1015", "100R@100MHz 0805"),
    "FB2": ("C1015", "100R@100MHz 0805"),
    "FB5V": ("C1015", "100R@100MHz 0805"),
    # electrolytics (Extended: none in the default library; ceramics would destabilise the XL1509 loop)
    "C4": ("C3350", "470uF 35V RVT1V471M1010"),
    "C2": ("C72494", "220uF 10V RVT1A221M0605"),
    "C22": ("C72494", "220uF 10V RVT1A221M0605"),
    # ceramics (Basic)
    "C16": ("C50254", "2.2uF 50V X7R 1206"),
    "C19": ("C50254", "2.2uF 50V X7R 1206"),
    "C3": ("C45783", "22uF 25V 0805"),
    "C7": ("C15850", "10uF 25V 0805"),
    "CLED3": ("C15850", "10uF 25V 0805"),
    **{r: ("C49678", "100nF 50V 0805") for r in ("C15", "C17", "C5", "C18", "CVDD1", "CNRST", "CM3", "CLED1", "CLED2")},
    # resistors (Basic, 0805 1%)
    "R47A": ("C17714", "47R 0805"),
    "R47B": ("C17714", "47R 0805"),
    "RDE": ("C17414", "10k 0805"),
    "RBZG": ("C149504", "100k 0805"),
    "RSTB": ("C17673", "4.7k 0805"),
    "RCLK": ("C17673", "4.7k 0805"),
    "RDIO": ("C17673", "4.7k 0805"),
    "RBIAS_A": ("C28636", "560R 0805"),
    "RBIAS_B": ("C28636", "560R 0805"),
    "RTERM": ("C17437", "120R 0805"),
}

# not assembled by JLC
NOT_ASSEMBLED = {"NT1", "JTERM", "J_SWD", "J_LOG"}
