# Startet ein Test-Exe LOSGELÖST von der aufrufenden Konsole und reicht den
# Exit-Code an ctest durch.
#
# Hintergrund (Windows): ctest ist eine Konsolen-App. Ein direkt gestartetes
# Test-Exe würde deren Konsole erben; die per ConPTY gestarteten Kind-Shells
# hängen sich dann an diese Konsole statt an die Pseudo-Konsole, und die
# PTY-E2E-Tests bekommen nie Ausgabe (bzw. hängen). Über Start-Process gestartet,
# hat das (GUI-Subsystem-)Test-Exe keine geerbte Konsole -> ConPTY bindet korrekt.
param(
    [Parameter(Mandatory = $true)][string]$Exe
)
$p = Start-Process -FilePath $Exe -PassThru -Wait -WindowStyle Hidden
exit $p.ExitCode
