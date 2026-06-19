# QTmux: Agenten-Ereignis aus einem HOOK melden — über das MCP-Tool 'post_event'
# (HTTP/JSON-RPC an 127.0.0.1). Dies ist der ZUVERLÄSSIGE Weg für KI-Agenten-Hooks
# (z. B. Claude Codes Stop-/Notification-Hook): deren stdout wird vom Agenten gekapselt,
# eine OSC-Ausgabe (qtmux-event) käme dort NICHT bei QTmux an — ein HTTP-Aufruf schon.
#
# Aufruf (typisch aus einem Hook):
#     qtmux-emit.ps1 done       "Aufgabe erledigt"
#     qtmux-emit.ps1 question   "Brauche eine Entscheidung"
#     qtmux-emit.ps1 error      "Build fehlgeschlagen"
#
# Die eigene Session-ID kommt aus $env:QTMUX_SESSION_ID (injiziert in jede QTmux-Shell;
# vom Agenten + dessen Hook-Subprozess geerbt). Fehlt sie, ordnet der Server über die
# Prozess-Vorfahrenkette zu.
param(
    [Parameter(Position=0)][string]$Kind = "info",
    [Parameter(Position=1)][string]$Text = "",
    [int]$Port = 7345
)

$argMap = @{ kind = $Kind; text = $Text }
if ($env:QTMUX_SESSION_ID) { $argMap.sessionId = [int]$env:QTMUX_SESSION_ID }
$body = @{ jsonrpc = "2.0"; id = 1; method = "tools/call";
           params = @{ name = "post_event"; arguments = $argMap } } |
        ConvertTo-Json -Depth 8 -Compress

# HttpClient mit UseProxy=$false → unabhängig von einem System-/Konzern-Proxy (squid),
# der 127.0.0.1 sonst abfangen könnte. Funktioniert in Windows PowerShell 5.1 und pwsh.
try {
    Add-Type -AssemblyName System.Net.Http -ErrorAction SilentlyContinue
    $handler = New-Object System.Net.Http.HttpClientHandler
    $handler.UseProxy = $false
    $client = New-Object System.Net.Http.HttpClient($handler)
    $client.Timeout = [TimeSpan]::FromSeconds(5)
    $content = New-Object System.Net.Http.StringContent($body, [System.Text.Encoding]::UTF8, "application/json")
    $null = $client.PostAsync("http://127.0.0.1:$Port/mcp", $content).GetAwaiter().GetResult()
    $client.Dispose()
} catch {
    # Hook soll den Agenten nie blockieren/fehlschlagen lassen.
}
