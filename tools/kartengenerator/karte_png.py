"""PNG lesen und schreiben (nur zlib und numpy) für den Kartengenerator.

Unterstützt 8 Bit je Kanal in den Farbtypen Graustufen (0), RGB (2) und RGBA (6), ohne
Zeilensprung. Der Schreiber erzeugt nur die Blöcke IHDR, IDAT und IEND (keine Zeitstempel,
kein Farbprofil); die Bytes hängen damit nur von den Pixeln und der zlib-Version ab.
"""

from __future__ import annotations

import struct
import zlib

import numpy as np

PNG_SIGNATUR = b"\x89PNG\r\n\x1a\n"
_M32 = 0xFFFFFFFF
#: Kanäle je Farbtyp.
_KANAELE = {0: 1, 2: 3, 6: 4}
#: zlib-Stufe der Ausgabe (9 ist bei den fast einfarbigen Kartenbildern kaum kleiner, aber langsamer).
ZLIB_STUFE = 6


def _chunk(typ: bytes, daten: bytes) -> bytes:
    return struct.pack(">I", len(daten)) + typ + daten + struct.pack(">I", zlib.crc32(typ + daten) & _M32)


def kodieren(bild: np.ndarray, stufe: int = ZLIB_STUFE) -> bytes:
    """Kodiert ein Bild (Höhe × Breite für Graustufen, Höhe × Breite × 3/4 für RGB/RGBA, uint8).

    Zeilenfilter je Zeile *Sub* oder *Up* (der mit weniger Bytes ≠ 0; bei Gleichstand *Up*,
    in der ersten Zeile *Sub*). Deterministisch.
    """
    bild = np.ascontiguousarray(bild, dtype=np.uint8)
    if bild.ndim == 2:
        farbtyp, kanaele = 0, 1
    elif bild.ndim == 3 and bild.shape[2] in (3, 4):
        kanaele = bild.shape[2]
        farbtyp = 2 if kanaele == 3 else 6
    else:
        raise ValueError(f"nicht unterstützte Bildform {bild.shape}")
    hoehe, breite = bild.shape[0], bild.shape[1]
    if hoehe < 1 or breite < 1:
        raise ValueError("leeres Bild")
    zeilen = bild.reshape(hoehe, breite * kanaele)
    sub = zeilen.copy()
    sub[:, kanaele:] -= zeilen[:, :-kanaele]  # uint8: Differenz modulo 256
    up = zeilen.copy()
    up[1:] -= zeilen[:-1]
    nz_sub = np.count_nonzero(sub, axis=1)
    nz_up = np.count_nonzero(up, axis=1)
    nimm_sub = nz_sub < nz_up
    nimm_sub[0] = True
    roh = np.empty((hoehe, breite * kanaele + 1), dtype=np.uint8)
    roh[:, 0] = np.where(nimm_sub, 1, 2)
    roh[:, 1:] = np.where(nimm_sub[:, None], sub, up)
    ihdr = struct.pack(">IIBBBBB", breite, hoehe, 8, farbtyp, 0, 0, 0)
    return (
        PNG_SIGNATUR
        + _chunk(b"IHDR", ihdr)
        + _chunk(b"IDAT", zlib.compress(roh.tobytes(), stufe))
        + _chunk(b"IEND", b"")
    )


def _paeth_zeile(z: bytearray, vorher: bytes, bpp: int) -> None:
    for k in range(len(z)):
        a = z[k - bpp] if k >= bpp else 0
        b = vorher[k]
        c = vorher[k - bpp] if k >= bpp else 0
        p = a + b - c
        pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
        if pa <= pb and pa <= pc:
            vorhersage = a
        elif pb <= pc:
            vorhersage = b
        else:
            vorhersage = c
        z[k] = (z[k] + vorhersage) & 0xFF


def _average_zeile(z: bytearray, vorher: bytes, bpp: int) -> None:
    for k in range(len(z)):
        a = z[k - bpp] if k >= bpp else 0
        z[k] = (z[k] + ((a + vorher[k]) >> 1)) & 0xFF


def dekodieren(daten: bytes) -> np.ndarray:
    """Liest ein PNG (8 Bit, Farbtyp 0/2/6, ohne Zeilensprung).

    Ergebnis: Höhe × Breite (Graustufen) bzw. Höhe × Breite × Kanäle, uint8. Wirft ``ValueError``
    bei ungültigen oder nicht unterstützten Dateien. Die Filter *Average* und *Paeth* sind in
    reinem Python implementiert (langsam); der Kartengenerator selbst schreibt sie nicht.
    """
    if not daten.startswith(PNG_SIGNATUR):
        raise ValueError("keine PNG-Datei")
    pos = len(PNG_SIGNATUR)
    ihdr: bytes | None = None
    idat = bytearray()
    ende = False
    while pos + 8 <= len(daten):
        laenge, typ = struct.unpack(">I4s", daten[pos : pos + 8])
        if pos + 12 + laenge > len(daten):
            raise ValueError("PNG abgeschnitten")
        inhalt = daten[pos + 8 : pos + 8 + laenge]
        (crc,) = struct.unpack(">I", daten[pos + 8 + laenge : pos + 12 + laenge])
        if zlib.crc32(typ + inhalt) & _M32 != crc:
            raise ValueError(f"Prüfsumme des PNG-Blocks {typ.decode('latin-1')} falsch")
        if typ == b"IHDR":
            ihdr = inhalt
        elif typ == b"IDAT":
            idat += inhalt
        elif typ == b"IEND":
            ende = True
            break
        pos += 12 + laenge
    if ihdr is None or len(ihdr) != 13:
        raise ValueError("PNG ohne gültigen IHDR-Block")
    if not ende:
        raise ValueError("PNG ohne IEND-Block (abgeschnitten?)")
    breite, hoehe, tiefe, farbtyp, kompression, filter_, zeilensprung = struct.unpack(">IIBBBBB", ihdr)
    if tiefe != 8 or farbtyp not in _KANAELE or (kompression, filter_, zeilensprung) != (0, 0, 0):
        raise ValueError("nicht unterstütztes PNG-Format (erwartet 8 Bit, Graustufen/RGB/RGBA, ohne Zeilensprung)")
    bpp = _KANAELE[farbtyp]
    schritt = bpp * breite
    try:
        roh = zlib.decompress(bytes(idat))
    except zlib.error as fehler:
        raise ValueError(f"PNG-Bilddaten nicht lesbar ({fehler})") from None
    if len(roh) != hoehe * (schritt + 1):
        raise ValueError("PNG-Bilddaten haben die falsche Länge")
    feld = np.frombuffer(roh, dtype=np.uint8).reshape(hoehe, schritt + 1)
    arten = feld[:, 0]
    if int(arten.max(initial=0)) > 4:
        raise ValueError(f"unbekannter PNG-Zeilenfilter {int(arten.max())}")
    bild = np.empty((hoehe, schritt), dtype=np.uint8)
    vorher = np.zeros(schritt, dtype=np.uint8)
    for y in range(hoehe):
        art = int(arten[y])
        z = feld[y, 1:]
        if art == 0:
            zeile = z
        elif art == 1:
            zeile = np.cumsum(z.reshape(breite, bpp), axis=0, dtype=np.uint8).reshape(schritt)
        elif art == 2:
            zeile = z + vorher
        else:
            puffer = bytearray(z.tobytes())
            if art == 3:
                _average_zeile(puffer, vorher.tobytes(), bpp)
            else:
                _paeth_zeile(puffer, vorher.tobytes(), bpp)
            zeile = np.frombuffer(bytes(puffer), dtype=np.uint8)
        bild[y] = zeile
        vorher = bild[y]
    if bpp == 1:
        return bild.reshape(hoehe, breite)
    return bild.reshape(hoehe, breite, bpp)
