#include <QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

#include "SecretsVault.h"
#include "SettingsIo.h"

using namespace qtmux;

/// Stufe 6 / Teil C4: Zurücksetzen, Export, Import.
///
/// Der Test läuft bewusst mit `QSettings::IniFormat` + `QStandardPaths`-Testmodus:
///  • hermetisch — er fasst weder die Registry noch die echten Einstellungen an,
///  • und INI ist der PESSIMISTISCHE Fall, weil dort jeder Wert als Text
///    zurückkommt. Wer nur gegen die Windows-Registry (typerhaltend) testet, merkt
///    nicht, dass ein `==` auf QVariant hier Änderungen melden würde, wo keine sind.
class tst_settingsio : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    void allowlistCoversSettingsAndExcludesState();
    void exportSkipsStateKeys();
    void exportNeverContainsSecrets();
    void roundTripExportResetImport();
    void resetCategoryTouchesOnlyThatPage();
    void resetAllKeepsWindowLayout();
    void importRejectsForeignFile();
    void importIgnoresUnknownKeysButShowsThem();

private:
    void seedSettings();
    QTemporaryDir m_dir;
};

void tst_settingsio::initTestCase() {
    QVERIFY(m_dir.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.path());
    QCoreApplication::setOrganizationName(QStringLiteral("QTmuxTest"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_settingsio"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.7.1"));
}

void tst_settingsio::cleanup() {
    QSettings s;
    s.clear();
    s.sync();
}

// Ein realistischer Stand: echte Einstellungen (Allowlist) UND Zustand, der NICHT
// mitgehen darf.
void tst_settingsio::seedSettings() {
    QSettings s;
    // Einstellungen
    s.setValue(QStringLiteral("ui/language"), QStringLiteral("de"));
    s.setValue(QStringLiteral("ui/themeMode"), 2);
    s.setValue(QStringLiteral("window/confirmQuit"), false);
    s.setValue(QStringLiteral("window/restoreSessionMode"), 1);
    s.setValue(QStringLiteral("window/terminalFontSize"), 15);
    s.setValue(QStringLiteral("window/terminalFontFamily"), QStringLiteral("FiraCode"));
    s.setValue(QStringLiteral("window/copyOnSelect"), true);
    s.setValue(QStringLiteral("window/resumeAgentMode"), 3);
    s.setValue(QStringLiteral("mcp/port"), 7346);
    s.setValue(QStringLiteral("colorSchemes/dark"), QStringLiteral("Nord"));
    s.setValue(QStringLiteral("hotkeys/toggleSidebar"), QStringLiteral("Ctrl+Shift+L"));
    // Zustand — Fenster-/Pane-Layout, Session-Liste, Geometrie, Ansichtszustand
    s.setValue(QStringLiteral("windows/size"), 2);
    s.setValue(QStringLiteral("windows/1/layout"), QStringLiteral("{\"paneId\":1}"));
    s.setValue(QStringLiteral("sessions/activeRow"), 1);
    s.setValue(QStringLiteral("window/x"), 120);
    s.setValue(QStringLiteral("window/width"), 1400);
    s.setValue(QStringLiteral("window/newSessionType"), 2);
    s.setValue(QStringLiteral("window/collapsedGroups"), QStringLiteral("[\"A\"]"));
    s.setValue(QStringLiteral("ui/sidebarWidth"), 300);
    s.setValue(QStringLiteral("ui/prefsCategory"), QStringLiteral("agenten"));
    s.sync();
}

// Jede Einstellung, die der Dialog anbietet, gehört zu genau einer Kategorie —
// sonst wäre sie über „Diese Seite zurücksetzen" nicht erreichbar. Und kein
// Zustands-Schlüssel gehört zu einer.
void tst_settingsio::allowlistCoversSettingsAndExcludesState() {
    const QStringList settings = {
        QStringLiteral("ui/language"), QStringLiteral("ui/themeMode"),
        QStringLiteral("window/confirmQuit"), QStringLiteral("window/restoreSessionMode"),
        QStringLiteral("window/quakeMode"), QStringLiteral("window/preventSleep"),
        QStringLiteral("window/terminalFontFamily"), QStringLiteral("window/terminalFontSize"),
        QStringLiteral("window/terminalLigatures"), QStringLiteral("window/terminalGpuRendering"),
        QStringLiteral("window/defaultShellProgram"),
        QStringLiteral("window/copyOnSelect"), QStringLiteral("window/rightClickPaste"),
        QStringLiteral("window/pasteWarnMultiline"),
        QStringLiteral("window/restoreAgents"), QStringLiteral("window/resumeAgentMode"),
        QStringLiteral("mcp/port"),
        QStringLiteral("colorSchemes/dark"), QStringLiteral("colorSchemes/light"),
        QStringLiteral("colorSchemes/imported"),
        QStringLiteral("hotkeys/actFind"), QStringLiteral("profiles/size"),
        QStringLiteral("profiles/1/name")
    };
    for (const QString &k : settings) {
        const QString cat = SettingsIo::categoryOf(k);
        QVERIFY2(!cat.isEmpty(), qPrintable(QStringLiteral("keiner Kategorie zugeordnet: %1").arg(k)));
        QVERIFY(SettingsIo::categories().contains(cat));
    }

    const QStringList state = {
        QStringLiteral("windows/size"), QStringLiteral("windows/1/layout"),
        QStringLiteral("windows/activeRow"), QStringLiteral("sessions/activeRow"),
        QStringLiteral("sessions/1/program"),
        QStringLiteral("window/x"), QStringLiteral("window/y"),
        QStringLiteral("window/width"), QStringLiteral("window/height"),
        QStringLiteral("window/newSessionType"), QStringLiteral("window/collapsedGroups"),
        QStringLiteral("ui/sidebarWidth"), QStringLiteral("ui/sidebarCollapsed"),
        QStringLiteral("ui/statusBarVisible"), QStringLiteral("ui/prefsCategory"),
        QStringLiteral("ui/prefsX"), QStringLiteral("ui/prefsWidth"),
        // Der Vault liegt als Datei außerhalb von QSettings; selbst ein Schlüssel
        // unter vault/ dürfte nie exportiert werden.
        QStringLiteral("vault/hint")
    };
    for (const QString &k : state)
        QVERIFY2(!SettingsIo::isExportable(k),
                 qPrintable(QStringLiteral("Zustand wird exportiert: %1").arg(k)));

    // Kategorien ohne Schlüssel sind bewusst leer, nicht vergessen.
    QVERIFY(SettingsIo::patternsFor(QStringLiteral("vault")).isEmpty());
    QVERIFY(SettingsIo::patternsFor(QStringLiteral("erweiterungen")).isEmpty());
}

void tst_settingsio::exportSkipsStateKeys() {
    seedSettings();
    SettingsIo io;
    const QJsonObject keys =
        QJsonDocument::fromJson(io.exportJson()).object().value(QStringLiteral("keys")).toObject();

    QVERIFY(keys.contains(QStringLiteral("window/terminalFontSize")));
    QVERIFY(keys.contains(QStringLiteral("hotkeys/toggleSidebar")));
    QCOMPARE(keys.value(QStringLiteral("mcp/port")).toVariant().toInt(), 7346);

    for (const QString &k : { QStringLiteral("windows/size"), QStringLiteral("windows/1/layout"),
                              QStringLiteral("sessions/activeRow"), QStringLiteral("window/x"),
                              QStringLiteral("window/width"), QStringLiteral("window/newSessionType"),
                              QStringLiteral("window/collapsedGroups"),
                              QStringLiteral("ui/sidebarWidth"), QStringLiteral("ui/prefsCategory") })
        QVERIFY2(!keys.contains(k), qPrintable(QStringLiteral("im Export: %1").arg(k)));
}

// Abnahmekriterium der Anweisung: „Vault-Schlüssel dürfen im Export NICHT vorkommen."
// Geprüft am Ergebnis, nicht an der Absicht: der Klartext eines echten Geheimnisses
// darf nirgends in den Bytes stehen — der PROFILNAME des Geheimnisses schon.
void tst_settingsio::exportNeverContainsSecrets() {
    SecretsVault *vault = SecretsVault::instance();
    const QString master = QStringLiteral("Testmeister-1");
    const QString secretValue = QStringLiteral("streng-geheimes-Kennwort-42");
    // Einen Vault aus einem früheren Lauf wegräumen: mit fremdem Master-Passwort
    // würden unlock UND create scheitern und der Test aus dem falschen Grund fehlen.
    // Pfad wie SecretsVault::filePath() (dort privat), unter QStandardPaths-Testmodus.
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                  + QStringLiteral("/vault.json"));
    QVERIFY(vault->create(master));
    QVERIFY(vault->setSecret(QStringLiteral("prod-ssh"), secretValue));

    QSettings s;
    s.beginWriteArray(QStringLiteral("profiles"), 1);
    s.setArrayIndex(0);
    s.setValue(QStringLiteral("name"), QStringLiteral("Buildserver"));
    s.setValue(QStringLiteral("host"), QStringLiteral("rtzbld01"));
    s.setValue(QStringLiteral("passwordSecret"), QStringLiteral("prod-ssh"));
    s.endArray();
    s.sync();

    SettingsIo io;
    const QByteArray json = io.exportJson();
    QVERIFY(json.contains("prod-ssh"));               // der NAME darf mit
    QVERIFY(!json.contains(secretValue.toUtf8()));    // der WERT nie
    QVERIFY(!json.contains(master.toUtf8()));

    vault->removeSecret(QStringLiteral("prod-ssh"));
    vault->lock();
}

// Der eigentliche Round-Trip: exportieren → alles zurücksetzen → importieren.
// Danach muss der Export byte-identisch sein.
void tst_settingsio::roundTripExportResetImport() {
    seedSettings();
    SettingsIo io;
    const QByteArray before = io.exportJson();
    const QUrl file = QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("export.json")));
    QVERIFY2(io.exportToFile(file), qPrintable(io.lastError()));

    const QStringList removed = io.resetAll();
    QVERIFY(removed.contains(QStringLiteral("window/terminalFontSize")));
    QVERIFY(io.exportableKeys().isEmpty());

    const QStringList changed = io.importFile(file);
    QVERIFY2(!changed.isEmpty(), qPrintable(io.lastError()));
    QCOMPARE(io.exportJson(), before);

    // Und die Typen stimmen wieder: 15 als Zahl, nicht als 15.0.
    QCOMPARE(QSettings().value(QStringLiteral("window/terminalFontSize")).toInt(), 15);
    QCOMPARE(QSettings().value(QStringLiteral("ui/language")).toString(), QStringLiteral("de"));

    // Ein zweiter Import derselben Datei ändert nichts mehr (idempotent).
    QVERIFY(io.importFile(file).isEmpty());
}

void tst_settingsio::resetCategoryTouchesOnlyThatPage() {
    seedSettings();
    SettingsIo io;
    const QStringList removed = io.resetCategory(QStringLiteral("eingabe"));
    QCOMPARE(removed, QStringList{ QStringLiteral("window/copyOnSelect") });

    QSettings s;
    QVERIFY(!s.contains(QStringLiteral("window/copyOnSelect")));
    QVERIFY(s.contains(QStringLiteral("window/terminalFontSize")));   // andere Seite
    QVERIFY(s.contains(QStringLiteral("hotkeys/toggleSidebar")));

    // Eine Kategorie ohne Schlüssel meldet ehrlich „nichts getan".
    QVERIFY(io.resetCategory(QStringLiteral("vault")).isEmpty());
    QVERIFY(io.resetCategory(QStringLiteral("erweiterungen")).isEmpty());
}

// Die wichtigste Schutzwirkung der Allowlist: „Alle Einstellungen zurücksetzen"
// darf die Sitzungsarbeit des Anwenders nicht mitnehmen.
void tst_settingsio::resetAllKeepsWindowLayout() {
    seedSettings();
    SettingsIo io;
    io.resetAll();

    QSettings s;
    QCOMPARE(s.value(QStringLiteral("windows/size")).toInt(), 2);
    QVERIFY(s.contains(QStringLiteral("windows/1/layout")));
    QCOMPARE(s.value(QStringLiteral("sessions/activeRow")).toInt(), 1);
    QCOMPARE(s.value(QStringLiteral("window/width")).toInt(), 1400);
    QCOMPARE(s.value(QStringLiteral("ui/sidebarWidth")).toInt(), 300);
    // …und die Einstellungen sind weg.
    QVERIFY(!s.contains(QStringLiteral("window/terminalFontSize")));
}

void tst_settingsio::importRejectsForeignFile() {
    SettingsIo io;
    const QString path = m_dir.filePath(QStringLiteral("fremd.json"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{ \"keys\": { \"window/terminalFontSize\": 99 } }");   // ohne Kennung
    f.close();

    QVERIFY(io.importFile(QUrl::fromLocalFile(path)).isEmpty());
    QVERIFY(!io.lastError().isEmpty());
    QVERIFY(!QSettings().contains(QStringLiteral("window/terminalFontSize")));

    // Kaputtes JSON und fehlende Datei ebenso — mit Meldung, nicht mit Absturz.
    QFile g(m_dir.filePath(QStringLiteral("kaputt.json")));
    QVERIFY(g.open(QIODevice::WriteOnly));
    g.write("{ das ist kein json");
    g.close();
    QVERIFY(io.importFile(QUrl::fromLocalFile(g.fileName())).isEmpty());
    QVERIFY(!io.lastError().isEmpty());
    QVERIFY(io.importFile(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("gibtsnicht.json")))).isEmpty());
    QVERIFY(!io.lastError().isEmpty());
}

// Eine Datei aus einer neueren Version bringt unbekannte Schlüssel mit: sie werden
// NICHT geschrieben, aber in der Vorschau ausgewiesen — still verschwinden wäre
// schlimmer als sichtbar übersprungen.
void tst_settingsio::importIgnoresUnknownKeysButShowsThem() {
    SettingsIo io;
    const QString path = m_dir.filePath(QStringLiteral("neuer.json"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{ \"format\": \"qtmux-settings\", \"formatVersion\": 1, \"keys\": {"
            " \"window/terminalFontSize\": 21,"
            " \"window/zukunftsschalter\": true,"
            " \"windows/size\": 9 } }");
    f.close();

    const QVariantList prev = io.importPreview(QUrl::fromLocalFile(path));
    QCOMPARE(prev.size(), 3);
    int skipped = 0;
    for (const QVariant &e : prev) {
        const QVariantMap m = e.toMap();
        if (m.value(QStringLiteral("skipped")).toBool()) skipped++;
        else QCOMPARE(m.value(QStringLiteral("to")).toString(), QStringLiteral("21"));
    }
    QCOMPARE(skipped, 2);   // zukunftsschalter + windows/size (Zustand)

    QCOMPARE(io.importFile(QUrl::fromLocalFile(path)),
             QStringList{ QStringLiteral("window/terminalFontSize") });
    QSettings s;
    QCOMPARE(s.value(QStringLiteral("window/terminalFontSize")).toInt(), 21);
    QVERIFY(!s.contains(QStringLiteral("window/zukunftsschalter")));
    QVERIFY(!s.contains(QStringLiteral("windows/size")));
}

QTEST_MAIN(tst_settingsio)
#include "tst_settingsio.moc"
