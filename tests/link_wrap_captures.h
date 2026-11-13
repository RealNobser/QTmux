// GENERIERT aus Mitschnitten (Auftrag Links/Umbruch 2026-11-13), nicht von Hand editieren.
// Quellen: Claude Code 2.1.323 in einer QTmux-Sitzung auf macOS (84x29, TERM=xterm-256color) und
// dieselbe Ausgabe bzw. cmd.exe in CreatePseudoConsole (Flags 0) unter Windows 10.0.26200.9711.
// Jeder Claude-Ausschnitt ist ein woertliches, zusammenhaengendes Stueck; er enthaelt nur die
// Probe-Adressen. Im cmd-Mitschnitt sind Domaene, Benutzer und Rechner LAENGENGLEICH ersetzt.
#pragma once
#include <cstddef>
namespace linkwrapcap {
struct Bytes { const char *bytes; std::size_t size; };
struct Segment { int cols; int rows; const char *bytes; std::size_t size; };
// Vollbild-Renderer, Codeblock, Adresse ueber drei Zeilen. cc-default.bin sha256 bebf4bd6fafdb8ecaa3223ec5ca44fc667394f714e26c2f639064d24c342a807, Offset 48622.
inline const Bytes kFullscreenCodeBlock = {
        "\033[38;5;153mhttps://example.com/C/n001/n002/n003/n004/n005/n006/n007/n008/n009/n010/n0"
        "11/n012/\r\033[2C\033[1Bn013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025"
        "/n026/n027/n028/n0\r\033[2C\033[1B29/n030\?fall=C&ende=1",
    214};
// Klassischer Renderer: Adresse endet genau am Rand, darunter beginnt ein neues Wort. cc-klassisch.bin sha256 18852b6fe02e7dc13b1db9755f0bb5dcbb4d25fc3cd068398597691a8df09db5, Offset 33486.
inline const Bytes kClassicWordAfterEdge = {
        "R1\033[6Gsiehe\033[12Ghttps://example.com/r1/s01/s02/s03/s04/s05/s06/s07/s08/s09/s10/s11"
        "/s12xxZ\r\033[2C\033[1Bdanach\033[10Gfolgt\033[16Gweiterer\033[25GText.",
    137};
// Klassischer Renderer, Codeblock: Adresse genau bis zum Rand, darunter eine ZWEITE Adresse. cc-klassisch.bin, Offset 35800.
inline const Bytes kClassicTwoUrlsBelowEachOther = {
        "\033[38;5;153mhttps://example.com/r4a/s01/s02/s03/s04/s05/s06/s07/s08/s09/s10/s11/s12/s1"
        "3/s14xxZ\r\033[1B\033[39m  \033[38;5;153mhttps://example.com/r4b/s01/s02/s03/s04Z\033[39"
        "m\033[K\r\033[1B  \033[38;5;153mwort",
    186};
// Klassischer Renderer, Werkzeugausgabe: Umbruch sechs Spalten vor dem Rand. cc-klassisch.bin, Offset 23134.
inline const Bytes kClassicToolOutput = {
        "https://example.com/D/n001/n002/n003/n004/n005/n006/n007/n008/n009/n010/n0\r\033[5C\033["
        "1B11/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025/n\r\033[1B "
        "\033[3G   026/n027/n028/n029/n030\?fall=D&ende=1",
    207};
// Vollbild-Renderer mit FORCE_HYPERLINK=1: dieselbe Adresse, je Zeile als OSC 8 mit gleicher id. cc-vollbild-osc8.bin sha256 45ca948c34d4a9884485de8fc7979385afbc3cee6693c177d0f260cca9385f56, Offset 40861.
inline const Bytes kFullscreenOsc8 = {
        "\033]8;id=uqdvn8;https://example.com/A/n001/n002/n003/n004/n005/n006/n007/n008/n009/n010"
        "/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025/n026/n027/n0"
        "28/n029/n030\?fall=A&ende=1\a\033[94mhttps://example.com/A/n001/n002/n003/n004/n005/n006"
        "/n007/n008/n0\033]8;;\a\r\033[1B\033[39m \033[3G\033]8;id=uqdvn8;https://example.com/A/n"
        "001/n002/n003/n004/n005/n006/n007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018"
        "/n019/n020/n021/n022/n023/n024/n025/n026/n027/n028/n029/n030\?fall=A&ende=1\a\033[94m09/"
        "n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025\033]8;;"
        "\a\r\033[2C\033[1B\033]8;id=uqdvn8;https://example.com/A/n001/n002/n003/n004/n005/n006/n"
        "007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024"
        "/n025/n026/n027/n028/n029/n030\?fall=A&ende=1\a/n026/n027/n028/n029/n030\?fall=A&ende=1"
        "\033]8;;\a",
    837};
// Klassischer Renderer mit FORCE_HYPERLINK=1, Werkzeugausgabe. cc-klassisch-osc8.bin sha256 f6b9dc75b544f4e2bd2d8a9c5b78867ddc81e6019a682d6b0609a641804a4673, Offset 30575.
inline const Bytes kClassicOsc8ToolOutput = {
        "\033]8;id=1xhoej8;https://example.com/D/n001/n002/n003/n004/n005/n006/n007/n008/n009/n01"
        "0/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025/n026/n027/n"
        "028/n029/n030\?fall=D&ende=1\a\033[94mhttps://example.com/D/n001/n002/n003/n004/n005/n00"
        "6/n007/n008/n009/n010/n0\033]8;;\a\r\033[5C\033[1B\033]8;id=1xhoej8;https://example.com/"
        "D/n001/n002/n003/n004/n005/n006/n007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n"
        "018/n019/n020/n021/n022/n023/n024/n025/n026/n027/n028/n029/n030\?fall=D&ende=1\a11/n012/"
        "n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025/n\033]8;;\a\r\033[1B"
        "\033[39m \033[3G   \033]8;id=1xhoej8;https://example.com/D/n001/n002/n003/n004/n005/n006"
        "/n007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n0"
        "24/n025/n026/n027/n028/n029/n030\?fall=D&ende=1\a\033[94m026/n027/n028/n029/n030\?fall=D"
        "&ende=1\033]8;;\a",
    843};
// Dieselbe Ausgabe durch ConPTY (Windows 10.0.26200.9711): OSC 8 kommt durch, mit eigener id und ST. b.out.bin sha256 1bb8ddd1a9d5d64a0547af1fcd2401f46e48df93a390d76e90ddfb78644eb74c, Offsets 9123 und 9523.
inline const Bytes kConPtyOsc8 = {
        "\033]8;id=u-uqdvn8%14709431822758253262;https://example.com/A/n001/n002/n003/n004/n005/n"
        "006/n007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023"
        "/n024/n025/n026/n027/n028/n029/n030\?fall=A&ende=1\033\\\033[20;21Hhttps://example.com/A"
        "/n001/n002/n003/n004/n005/n006/n007/n008/n0\033[21;3H09/n010/n011/n012/n013/n014/n015/n0"
        "16/n017/n018/n019/n020/n021/n022/n023/n024/n025\033[m\033]8;;\033\\\033]8;id=u-uqdvn8%14"
        "709431822758253262;https://example.com/A/n001/n002/n003/n004/n005/n006/n007/n008/n009/n0"
        "10/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025/n026/n027/"
        "n028/n029/n030\?fall=A&ende=1\033\\\033[22;3H/n026/n027/n028/n029/n030\?fall=A&ende=1"
        "\033[m\033]8;;\033\\",
    675};
// cmd.exe in ConPTY: `echo <Adresse>`, 84 -> 100 -> 84 Spalten, `echo ZWEI <Adresse> drei`,
// 84 -> 60 -> 84 Spalten. a.out.bin sha256 a59f590efa2ab89f70d25637cd52d760aa1551aa3dd5df243b2fa3145c2289f3.
inline const Segment kConPtyEchoResize[] = {
    {84, 29,
        "\033[\?9001h\033[\?1004h\033[\?25l\033[2J\033[m\033[HMicrosoft Windows [Version 10.0.262"
        "00.9711]\033]0;C:\\WINDOWS\\SYSTEM32\\cmd.exe\a\033[\?25h\033[\?25l\r\n(c) Microsoft Cor"
        "poration. Alle Rechte vorbehalten.\033[4;1Hexample0\\tester@WINHOST1 C:\\Users\\tester\\"
        "qtmux-linkwrap>\033]0;Administrator: C:\\WINDOWS\\SYSTEM32\\cmd.exe\a\033[\?25hecho http"
        "s://example.com/W/n001/n002/n003/n004/n005/n006/n007/n008/n009/n010/n011/n012/n013/n014/"
        "n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025/n026/n027/n028/n029/n030/ende\r\n"
        "\033[\?25lhttps://example.com/W/n001/n002/n003/n004/n005/n006/n007/n008/n009/n010/n011/n"
        "012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025/n026/n027/n028/n029"
        "/n030/ende\033[11;1Hexample0\\tester@WINHOST1 C:\\Users\\tester\\qtmux-linkwrap>\033[\?2"
        "5h",
        721},
    {100, 29,
        "\033[\?25l\033[8;29;100t\033[HMicrosoft Windows [Version 10.0.26200.9711]\033[K\r\n(c) M"
        "icrosoft Corporation. Alle Rechte vorbehalten.\033[K\r\n\033[K\r\nexample0\\tester@WINHO"
        "ST1 C:\\Users\\tester\\qtmux-linkwrap>echo https://example.com/W/n001/n002/n003/n004/n00"
        "5/n006/n007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n"
        "023/n024/n025/n026/n027/n028/n029/n030/ende\033[K\r\nhttps://example.com/W/n001/n002/n00"
        "3/n004/n005/n006/n007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n"
        "021/n022/n023/n024/n025/n026/n027/n028/n029/n030/ende\033[K\r\n\033[K\r\nexample0\\teste"
        "r@WINHOST1 C:\\Users\\tester\\qtmux-linkwrap>\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n"
        "\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r"
        "\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\033[10;57H\033[\?25"
        "h",
        725},
    {84, 29,
        "\033[\?25l\033[HMicrosoft Windows [Version 10.0.26200.9711]\033[K\r\n(c) Microsoft Corpo"
        "ration. Alle Rechte vorbehalten.\033[K\r\n\033[K\r\nexample0\\tester@WINHOST1 C:\\Users"
        "\\tester\\qtmux-linkwrap>echo https://example.com/W/n001/n002/n003/n004/n005/n006/n007/n"
        "008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025"
        "/n026/n027/n028/n029/n030/ende\033[K\r\nhttps://example.com/W/n001/n002/n003/n004/n005/n"
        "006/n007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023"
        "/n024/n025/n026/n027/n028/n029/n030/ende\033[K\r\n\033[K\r\nexample0\\tester@WINHOST1 C:"
        "\\Users\\tester\\qtmux-linkwrap>\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K"
        "\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033"
        "[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\033[11;57H\033[\?25hecho ZWEI https://example"
        ".com/W/n001/n002/n003/n004/n005/n006/n007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n"
        "017/n018/n019/n020/n021/n022/n023/n024/n025/n026/n027/n028/n029/n030/ende drei\r\n\033["
        "\?25lZWEI https://example.com/W/n001/n002/n003/n004/n005/n006/n007/n008/n009/n010/n011/n"
        "012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025/n026/n027/n028/n029"
        "/n030/ende drei\033[18;1Hexample0\\tester@WINHOST1 C:\\Users\\tester\\qtmux-linkwrap>"
        "\033[\?25h",
        1163},
    {60, 29,
        "\033[\?25l\033[HMicrosoft Windows [Version 10.0.26200.9711]\033[K\r\n(c) Microsoft Corpo"
        "ration. Alle Rechte vorbehalten.\033[K\r\n\033[K\r\nexample0\\tester@WINHOST1 C:\\Users"
        "\\tester\\qtmux-linkwrap>echo https://example.com/W/n001/n002/n003/n004/n005/n006/n007/n"
        "008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025"
        "/n026/n027/n028/n029/n030/ende   \r\nhttps://example.com/W/n001/n002/n003/n004/n005/n006"
        "/n007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n0"
        "24/n025/n026/n027/n028/n029/n030/ende    \r\n\033[K\r\nexample0\\tester@WINHOST1 C:\\Use"
        "rs\\tester\\qtmux-linkwrap>echo ZWEI https://example.com/W/n001/n002/n003/n004/n005/n006"
        "/n007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n0"
        "24/n025/n026/n027/n028/n029/n030/ende drei\033[K\r\nZWEI https://example.com/W/n001/n002"
        "/n003/n004/n005/n006/n007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n0"
        "20/n021/n022/n023/n024/n025/n026/n027/n028/n029/n030/ende drei\033[K\r\n\033[K\r\nexampl"
        "e0\\tester@WINHOST1 C:\\Users\\tester\\qtmux-linkwrap>    \r\n\033[K\r\n\033[K\r\n\033[K"
        "\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\033[22;57H\033[\?25h",
        1104},
    {84, 29,
        "\033[\?25l\033[HMicrosoft Windows [Version 10.0.26200.9711]\033[K\r\n(c) Microsoft Corpo"
        "ration. Alle Rechte vorbehalten.\033[K\r\n\033[K\r\nexample0\\tester@WINHOST1 C:\\Users"
        "\\tester\\qtmux-linkwrap>echo https://example.com/W/n001/n002/n003/n004/n005/n006/n007/n"
        "008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023/n024/n025"
        "/n026/n027/n028/n029/n030/ende\033[K\r\nhttps://example.com/W/n001/n002/n003/n004/n005/n"
        "006/n007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n023"
        "/n024/n025/n026/n027/n028/n029/n030/ende\033[K\r\n\033[K\r\nexample0\\tester@WINHOST1 C:"
        "\\Users\\tester\\qtmux-linkwrap>echo ZWEI https://example.com/W/n001/n002/n003/n004/n005"
        "/n006/n007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n019/n020/n021/n022/n0"
        "23/n024/n025/n026/n027/n028/n029/n030/ende drei     \r\nZWEI https://example.com/W/n001/"
        "n002/n003/n004/n005/n006/n007/n008/n009/n010/n011/n012/n013/n014/n015/n016/n017/n018/n01"
        "9/n020/n021/n022/n023/n024/n025/n026/n027/n028/n029/n030/ende drei\033[K\r\n\033[K\r\nex"
        "ample0\\tester@WINHOST1 C:\\Users\\tester\\qtmux-linkwrap>\033[K\r\n\033[K\r\n\033[K\r\n"
        "\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K\r\n\033[K"
        "\033[18;57H\033[\?25hexit\r\n\033]0;C:\\WINDOWS\\SYSTEM32\\cmd.exe\a\033[\?9001l\033[\?1"
        "004l",
        1178},
};
} // namespace linkwrapcap
