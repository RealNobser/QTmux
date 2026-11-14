// GENERIERT aus ConPTY-Mitschnitten (Auftrag ConPTY-Marken 2026-11-14), nicht von Hand editieren.
// Quelle: CreatePseudoConsole (Flags 0) unter Windows 10.0.26200.9711. Jedes Stueck ist woertlich;
// der Benutzername im Pfad ist LAENGENGLEICH ersetzt.
#pragma once
#include <cstddef>
namespace conptywrapcap {
struct Bytes { const char *bytes; std::size_t size; };
struct Segment { int cols; int rows; const char *bytes; std::size_t size; };
// Ein Probe-Programm schreibt zehn Zeilen und verlaesst sie auf zehn Arten (Zeilenvorschub, Cursor abwaerts, Cursor-Position, ...), Fenster 40 Spalten; danach 40 -> 60 -> 40 -> 30 -> 40. p1.out.bin sha256 61ada26d7748750eff0df9075f64b4fd9c313bd726f5c0fcb0af955422f50171.
inline const Segment kProbeFullRows[] = {
    {40, 40,
        "\033[\?9001h\033[\?1004h\033[2J\033[\?25l\033[2J\033[m\033[HP01 voll+CRLF\r\nAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\nnach01\r\nP02 voll+CR+CUD\r\nBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBnach02\r\nP03 voll+CUP\r\nCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCnach03\r"
        "\nP04 39+CR+CUD\r\nDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD\r\nnach04\r\nP05 echt 50\r\nE"
        "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\r\nnach05\r\nP06 voll+LF\r\nFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\r\nnach06\r\nP07 leer40+CR+CUD\r\n                       "
        "                 nach07\r\nP08 voll+CNL\r\nGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGnach0"
        "8\r\nP09 voll+EL+CRLF\r\nHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH\r\nnach09\r\nP10 voll+C"
        "R+LF getrennt\r\nIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII\r\nnach10\r\nENDE1\r\n\r\n\r\n"
        "\r\n\r\n\r\n\r\n\r\n\033[33;1H\033]0;C:\\Users\\tester\\qtmux-marken\\vtcat2.exe\a\033["
        "\?25h",
        777},
    {60, 40,
        "\033[\?25l\033[8;40;60t\033[HP01 voll+CRLF\033[K\r\nAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAA\033[K\r\nnach01\033[K\r\nP02 voll+CR+CUD\033[K\r\nBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBnach02\033[K\r\nP03 voll+CUP\033[K\r\nCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCnac"
        "h03\033[K\r\nP04 39+CR+CUD\033[K\r\nDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD\033[K\r\nnac"
        "h04\033[K\r\nP05 echt 50\033[K\r\nEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\033"
        "[K\r\nnach05\033[K\r\nP06 voll+LF\033[K\r\nFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\033["
        "K\r\nnach06\033[K\r\nP07 leer40+CR+CUD\033[K\r\n                                        "
        "nach07\033[K\r\nP08 voll+CNL\033[K\r\nGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGnach08\033"
        "[K\r\nP09 voll+EL+CRLF\033[K\r\nHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH\033[K\r\nnach09"
        "\033[K\r\nP10 voll+CR+LF getrennt\033[K\r\nIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII\033["
        "K\r\nnach10\033[K\r\nENDE1\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n"
        "\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\033[28;1H"
        "\033[\?25h",
        846},
    {40, 40,
        "\033[\?25l\033[HP01 voll+CRLF\033[K\r\nAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\nnach0"
        "1\033[K\r\nP02 voll+CR+CUD\033[K\r\nBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBnach02\033[K"
        "\r\nP03 voll+CUP\033[K\r\nCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCnach03\033[K\r\nP04 39"
        "+CR+CUD\033[K\r\nDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD \r\nnach04\033[K\r\nP05 echt 50"
        "\033[K\r\nEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\033[K\r\nnach05\033[K\r\nP0"
        "6 voll+LF\033[K\r\nFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\r\nnach06\033[K\r\nP07 leer4"
        "0+CR+CUD\033[K\r\n                                        nach07\033[K\r\nP08 voll+CNL"
        "\033[K\r\nGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGnach08\033[K\r\nP09 voll+EL+CRLF\033[K"
        "\r\nHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH \r\nnach09\033[K\r\nP10 voll+CR+LF getrennt"
        "\033[K\r\nIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII\r\nnach10\033[K\r\nENDE1\033[K\r\n"
        "\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\033[33;1H"
        "\033[\?25h",
        798},
    {30, 40,
        "\033[\?25l\033[HP01 voll+CRLF\033[K\r\nAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\033[K\r"
        "\nnach01\033[K\r\nP02 voll+CR+CUD\033[K\r\nBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBnach0"
        "2\033[K\r\nP03 voll+CUP\033[K\r\nCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCnach03\033[K\r"
        "\nP04 39+CR+CUD\033[K\r\nDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD\033[K\r\nnach04\033[K\r"
        "\nP05 echt 50\033[K\r\nEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\033[K\r\nnach0"
        "5\033[K\r\nP06 voll+LF\033[K\r\nFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\033[K\r\nnach06"
        "\033[K\r\nP07 leer40+CR+CUD\033[K\r\n                                        nach07\033["
        "K\r\nP08 voll+CNL\033[K\r\nGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGnach08\033[K\r\nP09 v"
        "oll+EL+CRLF\033[K\r\nHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH\033[K\r\nnach09\033[K\r\nP1"
        "0 voll+CR+LF getrennt       \r\nIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII\033[K\r\nnach10"
        "\033[K\r\nENDE1\033[K\r\n\033[K\r\n\033[K\r\n\033[K\033[38;1H\033[\?25h",
        790},
    {40, 40,
        "\033[\?25l\033[HP01 voll+CRLF\033[K\r\nAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\nnach0"
        "1\033[K\r\nP02 voll+CR+CUD\033[K\r\nBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBnach02\033[K"
        "\r\nP03 voll+CUP\033[K\r\nCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCnach03\033[K\r\nP04 39"
        "+CR+CUD\033[K\r\nDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD \r\nnach04\033[K\r\nP05 echt 50"
        "\033[K\r\nEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\033[K\r\nnach05\033[K\r\nP0"
        "6 voll+LF\033[K\r\nFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\r\nnach06\033[K\r\nP07 leer4"
        "0+CR+CUD\033[K\r\n                                        nach07\033[K\r\nP08 voll+CNL"
        "\033[K\r\nGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGnach08\033[K\r\nP09 voll+EL+CRLF\033[K"
        "\r\nHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH \r\nnach09\033[K\r\nP10 voll+CR+LF getrennt"
        "\033[K\r\nIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII\r\nnach10\033[K\r\nENDE1\033[K\r\n"
        "\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\033[33;1H"
        "\033[\?25h\033[\?9001l\033[\?1004l",
        814},
};
// Claude Code 2.1.323, klassischer Renderer, durch ConPTY bei 84 Spalten: die Eingabe des Anwenders, sechs Zeilen mit Hintergrund bis an den Rand. Am Mac verlaesst Claude Code jede Zeile mit CR und Cursor abwaerts; hier fehlt der Zeilenwechsel nach den ersten drei. b.out.bin sha256 1bb8ddd1a9d5d64a0547af1fcd2401f46e48df93a390d76e90ddfb78644eb74c, Offset 2015.
inline const Bytes kClaudeInputBox = {
        "\033[38;5;246m\033[48;5;237m\033[11;1H\342\235\257 \033[38;5;231mAntworte exakt mit dem "
        "folgenden Markdown zwischen den Marken ANFANG und ENDE     \033[39m  \033[38;5;231m(ohne"
        " die Marken, ohne Zusatz, URLs unver\303\244ndert und nicht als Markdown-Link       \033"
        "[39m  \033[38;5;231mformatiert). F\303\274hre DANACH genau einmal das Bash-Werkzeug mit "
        "dem Befehl \033[38;5;153mecho      \033[39m  \033[38;5;153m'https://example.com/D/n001/n"
        "002/n003/n004/n005/n006/n007/n008/n009/n010/n011/n01 \033[39m\r\n  \033[38;5;153m2/n013/"
        "n014/n015/n016/n017/n018\033[m\033[38;5;153m\033[48;5;237m/n019/n020/n021/n022/n023/n024"
        "/n025/n026/n027/n028 \033[39m\r\n  \033[38;5;153m/n029/n030\?fall=D&ende=1' \033[38;5;23"
        "1m(mit den einfachen Anf\303\274hrungszeichen) aus und schreibe  \033[39m\r\n",
    687};
// PowerShell 5.1 in ConPTY: Write-Output einer 176 Zeichen langen Adresse, 84 -> 100 -> 84 Spalten. d.out.bin sha256 19e6d2885c641d2af27496e138e379f9a48b2ce0156d3f4208260ade312c891f.
inline const Segment kPowerShellEchoResize[] = {
    {84, 29,
        "\033[\?9001h\033[\?1004h\033[\?25l\033[2J\033[m\033[H\033]0;Administrator: C:\\WINDOWS\\"
        "System32\\WindowsPowerShell\\v1.0\\powershell.exe\a\033[\?25hPS C:\\Users\\tester\\qtmux"
        "-linkwrap> \033[\?25l\033[93mW\033[\?25h\033[m\033[\?25l\033[1;34H> \033[93mWrite-Output"
        " \033[36m'https://example.com/W/n001/n002/n003/n004/n005/n006/n007/n008/n009/n010/n011/n"
        "012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025/n026/n027/n028/n029"
        "/n030/ende'\r\n\033[\?25h\033[mhttps://example.com/W/n001/n002/n003/n004/n005/n006/n007/"
        "n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n02"
        "5/n026/n027/n028/n029/n030/ende\r\nPS C:\\Users\\tester\\qtmux-linkwrap> ",
        611},
    {100, 29,
        "\033[\?25l\033[8;29;100t\033[HPS C:\\Users\\tester\\qtmux-linkwrap> \033[93mWrite-Output"
        " \033[36m'https://example.com/W/n001/n002/n003/n004/n005/n006/n007/n008/n009/n010/n011/n"
        "012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025/n026/n027/n028/n029"
        "/n030/ende'\033[K\033[m\r\nhttps://example.com/W/n001/n002/n003/n004/n005/n006/n007/n008"
        "/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025/n0"
        "26/n027/n028/n029/n030/ende\033[K\r\nPS C:\\Users\\tester\\qtmux-linkwrap>\033[K\r\n\033"
        "[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n"
        "\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r"
        "\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\033[6;36H\033[\?25h",
        610},
    {84, 29,
        "\033[\?25l\033[HPS C:\\Users\\tester\\qtmux-linkwrap> \033[93mWrite-Output \033[36m'http"
        "s://example.com/W/n001/n002/n003/n004/n005/n006/n007/n008/n009/n010/n011/n012/n013/n014/"
        "n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025/n026/n027/n028/n029/n030/ende'"
        "\033[K\033[m\r\nhttps://example.com/W/n001/n002/n003/n004/n005/n006/n007/n008/n009/n010/"
        "n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025/n026/n027/n02"
        "8/n029/n030/ende\033[K\r\nPS C:\\Users\\tester\\qtmux-linkwrap>\033[K\r\n\033[K\r\n\033["
        "K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n"
        "\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r"
        "\n\033[K\r\n\033[K\r\n\033[K\033[7;36H\033[\?25h\033[\?25l\033[92mexit\r\n\033[\?25h\033"
        "[\?9001l\033[\?1004l\033[m",
        636},
};
} // namespace conptywrapcap
