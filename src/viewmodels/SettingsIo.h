#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

namespace qtmux {

/// Zurücksetzen, Export und Import der Einstellungen (Design 1a, Stufe 6 / Teil C4).
/// In `main.cpp` als Context-Property `SettingsIo` registriert; die Kopfzeile des
/// Einstellungsfensters treibt darüber ihre beiden Textknöpfe.
///
/// **Kern des Entwurfs ist eine Allowlist.** Export UND Zurücksetzen arbeiten auf
/// genau derselben Schlüsselmenge — der Vereinigung der Kategorie-Muster unten.
/// Zwei Gründe, beide bewusst gegen den Wortlaut der Anweisung („alle Schlüssel der
/// Domain außer dem Vault"):
///  1. **Kein Leck durch Wachstum.** Eine Blocklist müsste bei jedem neuen Schlüssel
///     gepflegt werden; wird sie vergessen, landet er im Export — also womöglich ein
///     Pfad, ein Token oder ein Cache in einer Datei, die der Anwender weitergibt.
///     Eine Allowlist schweigt im Zweifel.
///  2. **Ein Reset darf keine Arbeit vernichten.** Unter `windows/*` und `sessions/*`
///     liegt das Fenster-/Pane-Layout samt Session-Liste (QTMUX-83) — Zustand, keine
///     Einstellung. „Alle Einstellungen zurücksetzen" darf das nicht anfassen.
/// Der Vault ist damit doppelt außen vor: er steht ohnehin nicht in QSettings, sondern
/// verschlüsselt in `vault.json` (s. SecretsVault::filePath) — und käme auch über die
/// Allowlist nicht hinein. Verbindungsprofile enthalten nur den NAMEN des Geheimnisses
/// (`passwordSecret`), niemals das Geheimnis selbst.
class SettingsIo : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
public:
    explicit SettingsIo(QObject *parent = nullptr);

    // --- Schlüsselmodell (statisch, ohne QSettings — direkt testbar) ---------
    /// IDs der Einstellungs-Kategorien in Rail-Reihenfolge (identisch zu
    /// `PrefsWindow.categories`; `vault` und `erweiterungen` haben bewusst keine
    /// Schlüssel, s. patternsFor).
    static QStringList categories();
    /// Schlüssel-Muster einer Kategorie. Endet ein Muster mit '/', ist es ein Präfix
    /// (`hotkeys/`, `profiles/`, `colorSchemes/`), sonst ein exakter Schlüssel.
    static QStringList patternsFor(const QString &category);
    static bool matchesCategory(const QString &key, const QString &category);
    /// Kategorie eines Schlüssels, "" wenn er zu keiner gehört (= wird weder
    /// exportiert noch zurückgesetzt).
    static QString categoryOf(const QString &key);
    static bool isExportable(const QString &key) { return !categoryOf(key).isEmpty(); }

    // --- Zugriff auf den aktuellen Stand ------------------------------------
    /// Vorhandene Schlüssel der Allowlist, sortiert.
    Q_INVOKABLE QStringList exportableKeys() const;
    /// Vorhandene Schlüssel einer Kategorie (für „Diese Seite zurücksetzen").
    Q_INVOKABLE QStringList keysForCategory(const QString &category) const;
    /// Aktueller Wert; ungültige QVariant (in QML `undefined`), wenn nicht gesetzt.
    /// QML nutzt das nach Reset/Import, um seine gebundenen Properties nachzuziehen.
    Q_INVOKABLE QVariant valueFor(const QString &key) const;

    // --- Export / Import ----------------------------------------------------
    /// JSON-Inhalt des Exports (auch ohne Datei — Grundlage für den Test).
    QByteArray exportJson() const;
    Q_INVOKABLE bool exportToFile(const QUrl &url);

    /// Vorschau des Imports: je Eintrag { key, from, to, skipped }. `skipped` markiert
    /// Schlüssel, die die Datei mitbringt, die aber nicht zur Allowlist gehören (z. B.
    /// aus einer neueren Version) — sie werden NICHT geschrieben, aber angezeigt,
    /// statt still zu verschwinden.
    Q_INVOKABLE QVariantList importPreview(const QUrl &url);
    /// Wendet die Datei an; liefert die tatsächlich geänderten Schlüssel (leer bei
    /// Fehler → `lastError`).
    Q_INVOKABLE QStringList importFile(const QUrl &url);

    // --- Zurücksetzen -------------------------------------------------------
    /// Entfernt die Schlüssel EINER Kategorie; liefert die entfernten.
    Q_INVOKABLE QStringList resetCategory(const QString &category);
    /// Entfernt alle Schlüssel der Allowlist; liefert die entfernten.
    Q_INVOKABLE QStringList resetAll();

    QString lastError() const { return m_lastError; }

signals:
    /// Die genannten Schlüssel haben einen neuen Wert (oder keinen mehr). QML zieht
    /// daraufhin seine gebundenen Properties nach.
    void changed(const QStringList &keys);
    void lastErrorChanged();

private:
    /// Liest + validiert die Datei; liefert die `keys`-Map. Bei Fehler leer + Meldung.
    QVariantMap readFile(const QUrl &url);
    /// Nach dem Schreiben: QSettings synchronisieren, betroffene Registries neu laden,
    /// `changed` melden.
    void afterWrite(const QStringList &keys);
    void setError(const QString &e);

    QString m_lastError;
};

} // namespace qtmux
