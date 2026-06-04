if(NOT DEFINED READELF OR NOT DEFINED ELF OR NOT DEFINED IMAGE_GEN OR NOT DEFINED IN_BIN OR NOT DEFINED OUT_EXE)
  message(FATAL_ERROR "Missing required variables: READELF, ELF, IMAGE_GEN, IN_BIN, OUT_EXE")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E env LC_ALL=C LANG=C ${READELF} -h ${ELF}
  OUTPUT_VARIABLE READELF_HEADER
  RESULT_VARIABLE READELF_RC
)

if(NOT READELF_RC EQUAL 0)
  message(FATAL_ERROR "Failed to read ELF header: ${ELF}")
endif()

string(REGEX MATCH "Entry point address:[ \t]*0x[0-9A-Fa-f]+" ENTRY_LINE "${READELF_HEADER}")
if(NOT ENTRY_LINE)
  message(FATAL_ERROR "Could not parse ELF entry point from readelf output")
endif()

string(REGEX REPLACE ".*(0x[0-9A-Fa-f]+).*" "\\1" ENTRY_ADDR "${ENTRY_LINE}")
string(STRIP "${ENTRY_ADDR}" ENTRY_ADDR)

execute_process(
  COMMAND ${IMAGE_GEN} -t exe -b ${ENTRY_ADDR} -i ${IN_BIN} -o ${OUT_EXE}
  RESULT_VARIABLE IMAGE_GEN_RC
)

if(NOT IMAGE_GEN_RC EQUAL 0)
  message(FATAL_ERROR "image_gen failed while creating EXE image")
endif()
