import os, sys, termios, tty

S = lambda code: f"\033[{code}m"
R, B, D, G, Y, BL, C, W, BB, BG = (
    S(c) for c in "0 1 2 31 32 33 34 36 37 44".split()
)
RESET, BOLD, DIM = R, B, D
RED, GREEN, YELLOW, BLUE, CYAN, WHITE = S("31"), S("32"), S("33"), S("34"), S("36"), S("37")
BG_BLUE, BG_GREEN, BG_RED, BG_YELLOW = S("44"), S("42"), S("41"), S("43")
BRIGHT = S("90")
WIDTH = 78


def _key() -> str:
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        ch = sys.stdin.read(1)
        return ch + sys.stdin.read(2) if ch == "\x1b" else ch
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)


def _ask(prompt: str, sensitive: bool = False) -> str:
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        if sensitive:
            new = termios.tcgetattr(fd)
            new[3] &= ~termios.ECHO
            termios.tcsetattr(fd, termios.TCSADRAIN, new)
        return sys.stdin.readline().strip()
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)


def clear() -> None:
    os.system("clear" if os.name == "posix" else "cls")


def banner() -> None:
    clear()
    nucleos = f"""{CYAN}{BOLD}
     ╔═══════════════════════════════════════════════════════════╗
     ║                                                          ║
     ║       {WHITE}███╗   ██╗██╗   ██╗ ██████╗███████╗██╗      {CYAN}║
     ║       {WHITE}████╗  ██║██║   ██║██╔════╝██╔════╝██║      {CYAN}║
     ║       {WHITE}██╔██╗ ██║██║   ██║██║     █████╗  ██║      {CYAN}║
     ║       {WHITE}██║╚██╗██║██║   ██║██║     ██╔══╝  ██║      {CYAN}║
     ║       {WHITE}██║ ╚████║╚██████╔╝╚██████╗███████╗███████╗ {CYAN}║
     ║       {WHITE}╚═╝  ╚═══╝ ╚═════╝  ╚═════╝╚══════╝╚══════╝ {CYAN}║
     ║                                                          ║
     ║              {GREEN}v3.0.0 — x86_64 — Installer{CYAN}                ║
     ╚═══════════════════════════════════════════════════════════╝
{R}"""
    print(nucleos)


def _line(ch: str = "─") -> str:
    return ch * WIDTH


def section(title: str) -> None:
    n = (WIDTH - len(title) - 6) // 2
    print(f"\n{BOLD}{CYAN}───{'─' * n} {title} {'─' * (WIDTH - len(title) - 6 - n)}───{R}\n")


def info(msg): print(f"  {BLUE}::{R} {msg}")
def ok(msg):   print(f"  {GREEN}✓{R} {msg}")
def warn(msg): print(f"  {YELLOW}⚠{R} {msg}")
def error(msg): print(f"  {RED}✗{R} {msg}")
def dim(msg):  print(f"  {DIM}{msg}{R}")


def step(cur: int, total: int, msg: str) -> None:
    print(f"\n  {BOLD}{BG_BLUE} {cur}/{total} {R} {BOLD}{msg}{R}\n")


def confirm(text: str, default: bool = True) -> bool:
    s = f"{BOLD}[{GREEN}Y{R}/{DIM}n{R}]{R}" if default else f"{BOLD}[{DIM}y{R}/{GREEN}N{R}]{R}"
    print(f"  {YELLOW}?{R} {text} {s} ", end="", flush=True)
    try:
        ch = _key()
        print(ch if ch != "\r" else "")
        return default if ch == "\r" else ch.lower() == "y"
    except:
        print()
        return default


def prompt(text: str, default: str | None = None) -> str:
    d = f" {DIM}[{default}]{R}" if default else ""
    print(f"  {YELLOW}?{R} {text}{d}")
    print(f"  {DIM}  └>{R} ", end="", flush=True)
    val = sys.stdin.readline().strip()
    return val or default or ""


def password(text: str) -> str:
    print(f"  {YELLOW}?{R} {text}")
    print(f"  {DIM}  └>{R} ", end="", flush=True)
    try:
        return _ask("", sensitive=True)
    finally:
        print()


def _nav_loop(title: str, options: list, render):
    if not options:
        return -1
    cur = 0
    while True:
        clear(); banner(); section(title)
        for i, opt in enumerate(options):
            render(i, opt, i == cur)
        print(f"\n  {DIM}↑↓ Enter q{R}")
        try:
            ch = _key()
        except:
            return -1 if title.startswith("Sel") else []
        if ch == "q": return -1
        if ch == "\x1b[A": cur = (cur - 1) % len(options)
        if ch == "\x1b[B": cur = (cur + 1) % len(options)
        if ch == "\r": return cur


def menu(title: str, options: list[tuple[str, str]], _: int = 0) -> int:
    def render(i, opt, active):
        k, d = opt
        print(f"  {BOLD if active else DIM}{GREEN if active else ''}▸ {k}{R}")
        print(f"    {DIM if active else ''}{d}{R}" if d else "")
    return _nav_loop(title, options, render)


def checkbox(title: str, options: list[tuple[str, str, bool]]) -> list[int]:
    sel = {i for i, (_, _, s) in enumerate(options) if s}
    cur = [0]

    def render(i, opt, active):
        k, d, _ = opt
        c = f"{GREEN}◉{R}" if i in sel else f"{DIM}○{R}"
        ptr = f"{GREEN}▸{R}" if active else " "
        print(f"  {ptr} {c} {k}\n    {DIM}{d}{R}")
    while True:
        clear(); banner(); section(title)
        for i, opt in enumerate(options):
            render(i, opt, i == cur[0])
        print(f"\n  {DIM}↑↓ Space Enter q{R}")
        try:
            ch = _key()
        except:
            return []
        if ch == "q": return []
        if ch == "\x1b[A": cur[0] = (cur[0] - 1) % len(options)
        if ch == "\x1b[B": cur[0] = (cur[0] + 1) % len(options)
        if ch == " ":
            sel.symmetric_difference_update({cur[0]})
        if ch == "\r": return sorted(sel)


def progress_bar(cur: int, total: int, prefix: str = "") -> None:
    n = int(40 * cur / total)
    bar = f"{GREEN}{'█' * n}{DIM}{'─' * (40 - n)}{R}"
    print(f"\r  {prefix}{bar} {100*cur//total}%", end="", flush=True)
    if cur >= total:
        print()


def run_with_progress(steps: list[tuple[str, callable]]) -> None:
    for i, (label, fn) in enumerate(steps, 1):
        step(i, len(steps), label)
        try:
            fn()
            ok(f"Completado: {label}")
        except Exception as e:
            error(f"Error en '{label}': {e}")
            raise
