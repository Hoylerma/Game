#pragma once

// Lader der Ereignisse (seit M11, docs/technik/ereignisse.md Abschnitt 3; chronik-skript.md 3.34). Läuft
// am Ende des Weltladers auf der fertigen Welt: Staaten, Gruppen, Strömungen, Parlamente, Güter, Orte und
// Gesetze sind dann bekannt.

#include "uhrwerk/loader.hpp"
#include "uhrwerk/world.hpp"

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace uhrwerk::detail {

/// Inhalt, den es gibt, aber nicht zum Startdatum: Schlüssel wie "staat:HAN" (ohne Orte zum Start),
/// "interessengruppe:arbeiterbewegung", "parlament:pr_abgeordnetenhaus" ('seit'). Verweise darauf sind
/// kein Fehler; die Bedingung ist dann nie wahr, die Wirkung entfällt.
using AbsentContent = std::set<std::string, std::less<>>;

/// Liest common/ereignisse.chron und common/ereignisse/*.chron in world.event_rules und world.events.
/// Ohne Regeldatei oder vor deren 'seit' bleibt event_rules.active false und es werden keine Ereignisse
/// gelesen. Meldungen gehen nach issues.
void load_event_content(const std::filesystem::path& data_dir, Date start, const AbsentContent& absent,
                        World& world, std::vector<LoadIssue>& issues);

} // namespace uhrwerk::detail
