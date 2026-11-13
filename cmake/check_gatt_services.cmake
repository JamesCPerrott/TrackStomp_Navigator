# Fail the pico2_w build if the generated GATT header names any service
# other than GAP, GATT, and BLE-MIDI (PRD §5.4). compile_gatt.py emits both
# ATT_SERVICE_<name>_START_HANDLE and ATT_SERVICE_<name>_01_START_HANDLE
# for the first instance of each service; the _NN suffix is the same service.

if (NOT GATT_HEADER)
    message(FATAL_ERROR "GATT_HEADER is unset")
endif()
if (NOT EXISTS "${GATT_HEADER}")
    message(FATAL_ERROR "GATT header not found: ${GATT_HEADER}")
endif()

file(READ "${GATT_HEADER}" GATT_TEXT)
string(REGEX MATCHALL "ATT_SERVICE_[A-Za-z0-9_]+_START_HANDLE" FOUND "${GATT_TEXT}")
if (NOT FOUND)
    message(FATAL_ERROR "No ATT_SERVICE_*_START_HANDLE macros in ${GATT_HEADER}")
endif()

set(SERVICES "")
foreach (MACRO_NAME IN LISTS FOUND)
    string(REGEX REPLACE "^ATT_SERVICE_" "" SERVICE "${MACRO_NAME}")
    string(REGEX REPLACE "_START_HANDLE$" "" SERVICE "${SERVICE}")
    # Instance suffix added for every service, including the first.
    string(REGEX REPLACE "_[0-9][0-9]$" "" SERVICE "${SERVICE}")
    list(APPEND SERVICES "${SERVICE}")
endforeach()
list(REMOVE_DUPLICATES SERVICES)
list(SORT SERVICES)

set(ALLOWED
    "03B80E5A_EDE8_4B33_A751_6CE34EC4C700"
    "GAP_SERVICE"
    "GATT_SERVICE"
)
list(SORT ALLOWED)

if (NOT SERVICES STREQUAL ALLOWED)
    message(FATAL_ERROR
        "GATT services in ${GATT_HEADER}:\n  ${SERVICES}\nexpected:\n  ${ALLOWED}")
endif()

if (NOT GATT_TEXT MATCHES "ATT_CHARACTERISTIC_7772E5DB_3868_4112_A1A9_F2669D106BF3_01_VALUE_HANDLE")
    message(FATAL_ERROR "BLE-MIDI value characteristic missing from ${GATT_HEADER}")
endif()
