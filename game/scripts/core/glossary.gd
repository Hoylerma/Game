class_name Glossary
extends RefCounted
## Kurze Erklärungen der Begriffe in den verschachtelten Tooltips (seit M13, Schritt a5c; vertical_slice.md
## 4.3). In Tooltips stehen sie als [url=begriff:<kennung>]…[/url]; das Lexikon (a5d) ergänzt längere
## Artikel. Inhalt nach Wissensstand, wie die übrigen Texte des Spiels.

const TERMS := {
	"kontribution": "Kontribution: direkte Steuer auf Grund und Boden und Gewerbe, die älteste und ergiebigste Abgabe; höhere Sätze drücken Einkommen und Lebensstandard.",
	"akzise": "Akzise: Verbrauchssteuer auf Waren des täglichen Bedarfs (Mahl- und Schlachtsteuer, Bier, Branntwein); sie verteuert die Güter in den Märkten.",
	"zoll": "Zoll: Abgabe auf Waren über die Grenze des Zollgebiets. Im Zollverein gilt der Vereinstarif, die Einnahmen werden nach der Bevölkerung geteilt.",
	"zollverein": "Zollverein: Zollunion unter preußischer Leitung seit 1834; ein Tarif, keine Binnenzölle, die Vereinskasse wird nach der Bevölkerung verteilt.",
	"schulden": "Staatsschulden: Anleihen des Staates; die Zinsen steigen mit der Schuldenquote, nach einem Staatsbankrott mit einem Aufschlag.",
	"saldo": "Saldo: Einnahmen weniger Ausgaben des Vormonats; ein Überschuss füllt den Schatz, ein Fehlbetrag zehrt ihn auf und führt in die Schulden.",
	"legitimitaet": "Legitimität: Zustimmung der Interessengruppen zur Regierung im Mittel, nach ihrem Anteil gewichtet (−10 bis +10). Sie dämpft die Unruhe.",
	"zustimmung": "Zustimmung: Haltung einer Interessengruppe zur Regierung von −10 bis +10; sie folgt den Gesetzen, die die Gruppe wünscht oder ablehnt, und den Ereignissen.",
	"unruhe": "Unruhe: 0 bis 100; sie nähert sich jeden Monat einem Zielwert aus Not, Teuerung, Verfassung, Zensur, der Unruhe der Nachbarn und der Zustimmung. Hohe Unruhe macht Aufstände wahrscheinlich.",
	"lebensstandard": "Lebensstandard: 0 bis 100; er folgt dem Realeinkommen – was nach den Steuern vom Einkommen bleibt, geteilt durch die Preise der Waren, die eine Schicht braucht. Teuerung und hohe Steuern senken ihn, und mit ihm sinken Geburten und Lebenserwartung.",
	"not": "In Not: Menschen, denen mehr als ein Viertel ihrer Grundnahrung fehlt (Getreide und was satt macht). Je größer die Lücke, desto mehr sterben – bei halber Deckung gut ein Drittel mehr als sonst.",
	"kreis": "Kreis: untere Verwaltungseinheit (Landkreis, Amt, Bezirk); die Karte zeigt sie im Modus „Kreise“.",
	"vermaechtnis": "Vermächtnis: wie die Geschichte den Staat beurteilt, in fünf Dimensionen von 0 bis 100; 1871 fließt es in das Urteil der Geschichte ein.",
	"einigkeit": "Einigkeit: Platz des Staates in einer deutschen Ordnung – Bund oder Reich, Präsidium, Zollverein – abzüglich Unruhe und Aufständen.",
	"recht": "Recht: Verfassung, Zustimmung der Gruppen und die Verlässlichkeit des Staates (ohne Bankrotte und Aufstände).",
	"freiheit": "Freiheit: Presse, Vereine, Grundlasten und Wahlrecht, dazu die Wahlen der letzten zehn Jahre.",
	"wohlstand": "Wohlstand: Lebensstandard und Alphabetisierung, abzüglich Auswanderung und Schuldenquote.",
	"geltung": "Geltung: Stärke des Heeres im Vergleich, Bündnisse, gewonnene und verlorene Kriege, Präsidium im Bund, Bevölkerung.",
}


static func text(key: String) -> String:
	return str(TERMS.get(key, ""))


## Verweis für einen Tooltip: [url=begriff:<kennung>]Wort[/url].
static func link(key: String, word: String) -> String:
	return "[url=begriff:%s]%s[/url]" % [key, word]
