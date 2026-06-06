#pragma once

#include <QList>
#include <QtGlobal>

namespace qtmux::procinfo {

/// PID des lokalen TCP-Clients, der den ephemeren Port `clientPort` belegt und
/// mit unserem Server-Port `serverPort` (auf 127.0.0.1) verbunden ist; -1 wenn
/// nicht gefunden. Dient dazu, den per MCP verbundenen Agentenprozess zu finden.
qint64 pidOfTcpClient(quint16 clientPort, quint16 serverPort);

/// Vorfahrenkette von `pid` (inkl. `pid` selbst), aufsteigend bis zur Wurzel.
/// Damit lässt sich ein verbundener Client-Prozess seiner QTmux-Session zuordnen
/// (Client → … → Shell-PID der Session). Nötig, weil aktuelle macOS-Versionen das
/// Lesen fremder Umgebungen (KERN_PROCARGS2) sperren.
QList<qint64> ancestorPids(qint64 pid);

/// Alle Nachfahren-PIDs von `root` (Kinder, Enkel … — ohne `root` selbst).
/// Muss aufgerufen werden, solange der Baum noch intakt ist (vor dem Beenden des
/// Wurzelprozesses, da Nachfahren danach zu init/launchd reparenten).
QList<qint64> descendantPids(qint64 root);

} // namespace qtmux::procinfo
