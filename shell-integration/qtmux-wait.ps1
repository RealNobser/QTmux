# QTmux: auf ein Agenten-Ereignis WARTEN — als Hintergrundprozess, der endet, sobald
# etwas vorliegt. Gegenstück zu qtmux-emit.ps1 (senden), Windows-Pendant zu qtmux-wait.sh.
#
# Warum das nötig ist: 'wait_for_events' ist ein Abholen (Long-Poll). Ein Ereignis
# erreicht einen Empfänger nur, WÄHREND der in diesem Aufruf wartet. Ein KI-Agent tut
# das praktisch nie: er arbeitet einen Zug ab und ruft Werkzeuge nur, wenn er sich dafür
# entscheidet — in einen laufenden Zug kann ein MCP-Server nicht hineinreichen. Ein
# Controller, der baut, verpasst deshalb jede Meldung, obwohl der Kanal intakt ist.
# Der Ausweg ist die einzige Stelle, an der ein Agent von außen geweckt werden kann:
# das ENDE eines Hintergrundbefehls. Dieses Skript blockiert also stellvertretend.
#
#     qtmux-wait.ps1
#     qtmux-wait.ps1 -After 45
#     qtmux-wait.ps1 -Sessions 2,3 -Kinds done,question
#     qtmux-wait.ps1 -MaxWait 600
#
# Bei Treffer: 'QTMUX EVENT seq=<n>' + Ereignis-JSON, Exit 0. Ohne Treffer nach dem
# Deckel: 'QTMUX TIMEOUT seq=<n>', ebenfalls Exit 0. Exit 2 nur bei Aufsatzfehlern.
# Das seq= der Abschlusszeile ist der Cursor für den nächsten Wächter (-After).
param(
    [int]$After = -1,                 # -1 = „ab jetzt" (Server-Vorgabe)
    [int[]]$Sessions = @(),
    [string[]]$Kinds = @(),
    [int]$MaxWait = 3000,             # ~50 min Deckel gegen vergessene Wächter
    [int]$Port = 0
)

if ($Port -le 0) {
    $Port = if ($env:QTMUX_MCP_PORT) { [int]$env:QTMUX_MCP_PORT } else { 7345 }
}
$url = "http://127.0.0.1:$Port/mcp"

if (-not $env:QTMUX_SESSION_ID) {
    Write-Error "qtmux-wait: `$env:QTMUX_SESSION_ID fehlt — in einer QTmux-Shell starten."
    exit 2
}
$sid = [int]$env:QTMUX_SESSION_ID

# HttpClient mit UseProxy=$false — wie qtmux-emit.ps1, damit ein System-/Konzern-Proxy
# 127.0.0.1 nicht abfängt. Funktioniert in Windows PowerShell 5.1 und pwsh.
Add-Type -AssemblyName System.Net.Http -ErrorAction SilentlyContinue
$handler = New-Object System.Net.Http.HttpClientHandler
$handler.UseProxy = $false
$client = New-Object System.Net.Http.HttpClient($handler)

function Invoke-Tool([string]$Name, [hashtable]$Arguments, [int]$TimeoutSec) {
    $body = @{ jsonrpc = "2.0"; id = 1; method = "tools/call";
               params = @{ name = $Name; arguments = $Arguments } } |
            ConvertTo-Json -Depth 8 -Compress
    try {
        $client.Timeout = [TimeSpan]::FromSeconds($TimeoutSec)
        $content = New-Object System.Net.Http.StringContent($body, [System.Text.Encoding]::UTF8, "application/json")
        $resp = $client.PostAsync($url, $content).GetAwaiter().GetResult()
        $raw  = $resp.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        # Die Nutzlast steckt als JSON-Zeichenkette im Textfeld → zweimal auspacken.
        return ($raw | ConvertFrom-Json).result.content[0].text | ConvertFrom-Json
    } catch {
        return $null
    }
}

# --- Abo sicherstellen -------------------------------------------------------------
# wait_for_events OHNE Abo antwortet sofort mit einem Fehler — ein Wächter würde dann in
# einer heißen Schleife den Server hämmern statt zu warten. Ein Abo je Subscriber-Session,
# ein neues ERSETZT das alte; ohne -Sessions/-Kinds fassen wir ein vorhandenes nicht an.
$subs = Invoke-Tool "list_subscriptions" @{} 5
if ($null -eq $subs) {
    Write-Error "qtmux-wait: kein Kontakt zum MCP-Server auf $url"
    exit 2
}
$hasSub = @($subs | Where-Object { $_.subscriberSessionId -eq $sid }).Count -gt 0

if ($Sessions.Count -gt 0 -or $Kinds.Count -gt 0 -or -not $hasSub) {
    $subArgs = @{ sessionId = $sid }
    if ($Sessions.Count -gt 0) { $subArgs.sources = $Sessions }
    if ($Kinds.Count -gt 0)    { $subArgs.kinds   = $Kinds }
    $null = Invoke-Tool "subscribe_events" $subArgs 5
}

# --- warten ------------------------------------------------------------------------
$deadline = (Get-Date).AddSeconds($MaxWait)
$cursor   = $After

while ((Get-Date) -lt $deadline) {
    # Poll auf die Restzeit kürzen, sonst überzieht der Deckel um eine volle Poll-Länge.
    $remainMs = [int]((($deadline) - (Get-Date)).TotalMilliseconds)
    $pollMs   = [Math]::Max(1000, [Math]::Min(45000, $remainMs))
    # Das HTTP-Timeout MUSS über timeoutMs liegen, sonst schneidet der Client den
    # Long-Poll ab, BEVOR der Server antwortet — der Wächter verlöre genau das Ereignis.
    $pollSec  = [int]($pollMs / 1000) + 10

    $waitArgs = @{ sessionId = $sid; timeoutMs = $pollMs }
    if ($cursor -ge 0) { $waitArgs.afterSeq = $cursor }

    $res = Invoke-Tool "wait_for_events" $waitArgs $pollSec
    if ($null -eq $res) { continue }        # Abbruch/Netzhänger: neu pollen

    # Der Cursor wird IMMER fortgeschrieben — auch ohne Treffer. Sonst pollt der
    # Wächter endlos über dieselben (herausgefilterten) Ereignisse.
    if ($null -ne $res.nextSeq) { $cursor = [int]$res.nextSeq }

    if ($res.error) {
        Write-Error "qtmux-wait: Server meldet einen Fehler: $($res.error)"
        exit 2
    }

    # Serverseitig gefiltert: was hier ankommt, passt zum Abo.
    if ($res.events -and @($res.events).Count -gt 0) {
        Write-Output "QTMUX EVENT seq=$cursor"
        Write-Output ($res | ConvertTo-Json -Depth 8 -Compress)
        exit 0
    }
}

Write-Output "QTMUX TIMEOUT seq=$cursor — kein Ereignis innerhalb von ${MaxWait}s"
exit 0
