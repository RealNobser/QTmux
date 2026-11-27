#pragma once

#include <QByteArray>
#include <QList>
#include <QtGlobal>

namespace qtmux::procinfo {

/// PID des lokalen TCP-Clients, der den ephemeren Port `clientPort` belegt und
/// mit unserem Server-Port `serverPort` (auf 127.0.0.1) verbunden ist; -1 wenn
/// nicht gefunden. Dient dazu, den per MCP verbundenen Agentenprozess zu finden.
qint64 pidOfTcpClient(quint16 clientPort, quint16 serverPort);

/// Gehört der lokale TCP-Client am Port `clientPort` (verbunden mit `serverPort`) zum
/// Prozessbaum unter `rootPid` (die Wurzel eingeschlossen)? Dieselbe Frage wie
/// `pidOfTcpClient` + `ancestorPids`, aber als PRÜFUNG gegen einen bekannten Besitzer:
/// Unter macOS und Linux werden nur die Sockets der Nachfahren angesehen statt die aller
/// Prozesse. Gedacht für Anfragen, die oft kommen (Abfragen eines MCP-Tasks alle paar
/// Sekunden), und die im GUI-Thread laufen.
bool tcpClientBelongsTo(qint64 rootPid, quint16 clientPort, quint16 serverPort);

/// Vorfahrenkette von `pid` (inkl. `pid` selbst), aufsteigend bis zur Wurzel.
/// Damit lässt sich ein verbundener Client-Prozess seiner QTmux-Session zuordnen
/// (Client → … → Shell-PID der Session). Nötig, weil aktuelle macOS-Versionen das
/// Lesen fremder Umgebungen (KERN_PROCARGS2) sperren.
QList<qint64> ancestorPids(qint64 pid);

/// Alle Nachfahren-PIDs von `root` (Kinder, Enkel … — ohne `root` selbst).
/// Muss aufgerufen werden, solange der Baum noch intakt ist (vor dem Beenden des
/// Wurzelprozesses, da Nachfahren danach zu init/launchd reparenten).
QList<qint64> descendantPids(qint64 root);

/// Bausteine des Linux-Wegs, die sich ohne /proc prüfen lassen (Tests auf jedem Unix).
namespace detail {

/// Inode der Verbindung `clientPort` → `serverPort` in einer Kernel-Tabelle im Format von
/// /proc/net/tcp bzw. /proc/net/tcp6 (Kopfzeile, dann je Verbindung eine Zeile); -1 = keine.
qint64 tcpTableInode(const QByteArray &table, quint16 clientPort, quint16 serverPort);

/// Enthält das Verzeichnis `dir` einen Symlink, dessen Ziel WÖRTLICH `target` ist (etwa
/// „socket:[4711]“ in /proc/<pid>/fd)? Unter Windows immer false.
bool dirHasLinkTo(const QByteArray &dir, const QByteArray &target);

} // namespace detail

} // namespace qtmux::procinfo
