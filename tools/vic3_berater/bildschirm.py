"""Screenshot des Spiels aufnehmen und für die Anfrage verkleinern (mss und Pillow)."""

from __future__ import annotations

import io

# Grenzen der hochauflösenden Bildeingabe von Claude: 2576 px an der langen Kante, 3,75 MP.
MAX_KANTE = 2576
MAX_PIXEL = 3_750_000


def zielgroesse(breite: int, hoehe: int, max_kante: int = MAX_KANTE) -> tuple[int, int]:
    """Größe nach dem Verkleinern; Seitenverhältnis bleibt, vergrößert wird nie."""
    faktor = min(1.0, max_kante / max(breite, hoehe), (MAX_PIXEL / (breite * hoehe)) ** 0.5)
    return max(1, int(breite * faktor)), max(1, int(hoehe * faktor))


def aufnehmen(monitor: int = 1, max_kante: int = MAX_KANTE, qualitaet: int = 85) -> bytes:
    """Screenshot des Monitors (1 = Hauptmonitor, 0 = alle) als JPEG."""
    import mss
    from PIL import Image

    neu = getattr(mss, "MSS", None)  # mss.mss() ist in neueren Versionen veraltet
    with (neu or mss.mss)() as sct:
        if not 0 <= monitor < len(sct.monitors):
            raise ValueError(f"Monitor {monitor} gibt es nicht (vorhanden: 0 bis {len(sct.monitors) - 1}).")
        roh = sct.grab(sct.monitors[monitor])
        bild = Image.frombytes("RGB", roh.size, roh.bgra, "raw", "BGRX")
    groesse = zielgroesse(*bild.size, max_kante=max_kante)
    if groesse != bild.size:
        bild = bild.resize(groesse, Image.LANCZOS)
    puffer = io.BytesIO()
    bild.save(puffer, format="JPEG", quality=qualitaet)
    return puffer.getvalue()
