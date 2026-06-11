#pragma once

// Plattformübergreifende Hilfs-Kommandos für die E2E-Tests.
//
// Die Tests starten echte Prozesse durch den PTY-Layer und prüfen die Kette
// PTY -> libvterm -> Session. Die dafür nötigen "Werkzeug"-Prozesse unterscheiden
// sich je Plattform (Unix: /bin/echo, /usr/bin/printf, /bin/sh; Windows: cmd.exe,
// powershell.exe). Dieser Header kapselt die Unterschiede, damit die Tests selbst
// plattformneutral bleiben. Das Unix-Verhalten entspricht 1:1 den ursprünglichen
// (auf macOS verifizierten) Kommandos.

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace qtmux_test {

struct Cmd {
    QString     program;
    QStringList args;
};

// Schreibt `text` + Zeilenende auf stdout und beendet sich.
inline Cmd printLine(const QString &text) {
#if defined(Q_OS_WIN)
    return {QStringLiteral("cmd.exe"), {QStringLiteral("/c"), QStringLiteral("echo"), text}};
#else
    return {QStringLiteral("/bin/echo"), {text}};
#endif
}

#if defined(Q_OS_WIN)
// Baut einen PowerShell-Ausdruck, der `bytes` exakt (ohne zusätzliches Zeilenende)
// auf stdout schreibt: [Console]::Out.Write([char]27 + [char]93 + ...).
inline QString psWriteExpr(const QByteArray &bytes) {
    QStringList parts;
    for (unsigned char b : bytes)
        parts << QStringLiteral("[char]%1").arg(int(b));
    return QStringLiteral("[Console]::Out.Write(%1)").arg(parts.join(QStringLiteral(" + ")));
}
#endif

// Schreibt beliebige Bytes (inkl. Escape/BEL) exakt auf stdout und beendet sich.
inline Cmd emitRaw(const QByteArray &bytes) {
#if defined(Q_OS_WIN)
    return {QStringLiteral("powershell.exe"),
            {QStringLiteral("-NoProfile"), QStringLiteral("-Command"), psWriteExpr(bytes)}};
#else
    // printf interpretiert die Oktal-Escapes; Bytes als \NNN übergeben.
    QString fmt;
    for (unsigned char b : bytes)
        fmt += QStringLiteral("\\%1").arg(int(b), 3, 8, QLatin1Char('0'));
    return {QStringLiteral("/bin/sh"), {QStringLiteral("-c"),
            QStringLiteral("printf '%1'").arg(fmt)}};
#endif
}

// Schreibt Bytes exakt auf stdout und bleibt danach `seconds` Sekunden am Leben
// (damit ein gesetzter Zustand nicht sofort von "Closed" überschrieben wird).
inline Cmd emitRawThenWait(const QByteArray &bytes, int seconds) {
#if defined(Q_OS_WIN)
    const QString expr = psWriteExpr(bytes)
        + QStringLiteral("; Start-Sleep -Seconds %1").arg(seconds);
    return {QStringLiteral("powershell.exe"),
            {QStringLiteral("-NoProfile"), QStringLiteral("-Command"), expr}};
#else
    QString fmt;
    for (unsigned char b : bytes)
        fmt += QStringLiteral("\\%1").arg(int(b), 3, 8, QLatin1Char('0'));
    return {QStringLiteral("/bin/sh"), {QStringLiteral("-c"),
            QStringLiteral("printf '%1'; sleep %2").arg(fmt).arg(seconds)}};
#endif
}

// Interaktive Shell für Eingabe-Echo-Tests. Unix: leerer Programmname ->
// PtyBackend::defaultShell() (zsh/bash). Windows: cmd.exe statt PowerShell —
// startet praktisch sofort, echot die Eingabe und führt sie bei CR aus, ohne
// PSReadLine-Eigenheiten/Startverzögerung.
inline Cmd interactiveShell() {
#if defined(Q_OS_WIN)
    return {QStringLiteral("cmd.exe"), {}};
#else
    return {QString(), {}};
#endif
}

// Fragt "Password: " ab, liest eine Zeile und echot sie als "PWGOT:<wert>" —
// simuliert die SSH-Passwort-Eingabeaufforderung für den Vault-Auto-Fill-Test.
inline Cmd passwordPrompt() {
#if defined(Q_OS_WIN)
    return {QStringLiteral("cmd.exe"),
            {QStringLiteral("/c"),
             QStringLiteral("set /p p=Password: & echo PWGOT:%p%")}};
#else
    return {QStringLiteral("/bin/sh"),
            {QStringLiteral("-c"),
             QStringLiteral("printf 'Password: '; read p; echo PWGOT:$p")}};
#endif
}

// Tastendruck "Enter", wie ihn das echte Terminal sendet (CR, plattformneutral:
// der Unix-PTY mappt CR via ICRNL auf NL).
inline QByteArray enterKey() { return QByteArrayLiteral("\r"); }

} // namespace qtmux_test
