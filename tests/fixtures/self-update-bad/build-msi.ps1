# Gegenprobe fuer test_installer_self_update_contract_bites (nie ausgefuehrt):
# Alle vier Helfer-Zeilen stehen hier nur als Kommentar und duerfen nicht zaehlen.
#   $helper = Join-Path $build "update-helper.exe"
#   throw "update-helper.exe fehlt neben qtmux.exe"
#   Copy-Item $helper $stage
#   foreach ($need in @("qtmux.exe", "update-helper.exe")) { }
Copy-Item (Join-Path $build "qtmux.exe") $stage
