# MCP-Port auf Netzebene einschränken (macOS/pf)

Zweite Schicht unter dem Zugriffs-Token des MCP-Servers (QTMUX-127): Das Token
entscheidet, **wer etwas darf**; pf entscheidet, **wer überhaupt anklopfen darf**.
Beides zusammen — die Firewall ersetzt das Token nicht, und das Token ersetzt die
Firewall nicht.

> Die macOS **Application Firewall** (Systemeinstellungen → Netzwerk → Firewall) kann
> das nicht: Sie kennt Programme, keine Quellnetze. Für „nur aus 192.168.0.0/24" ist
> `pf` das Mittel.

## Einrichten

```bash
sudo tools/pf/install-pf-anchor.sh                       # Port 7345, 192.168.0.0/24
sudo tools/pf/install-pf-anchor.sh --port 7346 --net 10.0.0.0/8
sudo tools/pf/install-pf-anchor.sh --uninstall
```

Das Skript legt an:

| Datei | Zweck |
|---|---|
| `/usr/local/etc/qtmux/qtmux-mcp.pf.conf` | die Regeln (block · pass aus dem Netz · pass von Loopback) |
| `/usr/local/etc/qtmux/pf.conf` | Kopie der aktuellen `/etc/pf.conf` **plus** unser Anchor |
| `/Library/LaunchDaemons/com.qtmux.pf.plist` | lädt das Ganze beim Systemstart |

## Zwei Fallen, die eine Sperre vortäuschen

🔑 **Ein frei geladenes Anchor greift nicht.** `pfctl -a com.qtmux -f regeln.conf`
legt die Regeln zwar ab, aber pf durchläuft ein Anchor nur, wenn die **geladene
Regelmenge** es mit `anchor "com.qtmux"` einhängt. Apples `/etc/pf.conf` kennt
ausschließlich `com.apple/*`. Ohne den Einhängepunkt zeigt `pfctl -a com.qtmux -s
rules` die Regeln an — und trotzdem kommt jeder durch. Deshalb die eigene
Hauptkonfiguration.

🔑 **`/etc/pf.conf` selbst zu ändern hält nicht.** macOS-Updates ersetzen die Datei
kommentarlos; die Regel ist danach still weg. Das Skript erzeugt seine Kopie darum
bei **jedem** Lauf neu aus dem aktuellen Original — nach einem größeren Systemupdate
einfach noch einmal aufrufen.

## Prüfen

```bash
sudo pfctl -s info | head -3          # Status: Enabled
sudo pfctl -a com.qtmux -s rules      # unsere drei Regeln
sudo pfctl -s rules | grep com.qtmux  # ⚠️ der wichtige Nachweis: das Anchor ist eingehängt
```

Der einzige belastbare Test ist die **Gegenprobe von außen**: aus dem erlaubten Netz
muss `curl http://<host>:7345/` antworten (401 ohne Token zählt als „angekommen"),
von außerhalb muss es in einen **Timeout** laufen — nicht in „connection refused",
denn das käme vom Rechner selbst und hieße, dass pf gar nicht greift.

## Reihenfolge der Regeln

pf wertet „last match wins" aus, deshalb steht `block` **vor** den `pass`-Zeilen und
es wird bewusst **kein** `quick` verwendet. Wer die Datei bearbeitet: erst blocken,
dann durchlassen — umgekehrt ist alles offen.
