# Führt ein Testprogramm aus und druckt bei Rot alles, was zur Diagnose gebraucht wird
# (ein CI-Lauf endete unter Windows rot, ohne dass ctest eine Zeile Ausgabe zeigte).
#
#   TEST        das Testprogramm
#   QTEST_FILE  Datei, in die QTest zusätzlich schreibt (-o <datei>,txt)
#   LOGDIR      Ordner mit den Logs der Kindprozesse (*.log), bei Rot gedruckt
#   ARGS        optional: Argumente vor den -o-Schaltern (z. B. ein Testfunktionsname)
#
# Der Exit-Code kommt von execute_process: eine Zahl, oder bei einem abgestürzten
# Prozess ein Text wie „Exit code 0xc0000005“ (Windows) bzw. „Segmentation fault“.
# Ausgaben werden immer weitergereicht (ctest zeigt sie bei Rot oder mit -V).

cmake_minimum_required(VERSION 3.24)

foreach(_v TEST QTEST_FILE LOGDIR)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "RunAndReport: -D${_v}=… fehlt")
    endif()
endforeach()

file(REMOVE "${QTEST_FILE}")
execute_process(
    COMMAND "${TEST}" ${ARGS} -o "${QTEST_FILE},txt" -o "-,txt"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err)

message("${_out}")
message("${_err}")
if(_rc STREQUAL "0")
    return()
endif()

message("==== RunAndReport: ${TEST} endete mit: ${_rc}")
if(EXISTS "${QTEST_FILE}")
    file(READ "${QTEST_FILE}" _qtest)
    message("==== QTest-Datei ${QTEST_FILE}:\n${_qtest}")
else()
    message("==== QTest-Datei ${QTEST_FILE} fehlt (Programm endete vor der ersten Zeile?)")
endif()
file(GLOB _logs "${LOGDIR}/*.log")
list(SORT _logs)
foreach(_log IN LISTS _logs)
    file(READ "${_log}" _text)
    message("==== Log ${_log}:\n${_text}")
endforeach()
message(FATAL_ERROR "RunAndReport: rot (${_rc})")
