PROJECT("grainflow_record_tilde")


include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../GrainflowLib/include")

set(
    SOURCE_FILES
	${PROJECT_NAME}.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/PdBufferReader.h
)

ADD_PD_EXTERNAL(
 ${PROJECT_NAME}
 grainflow.record~
 ${SOURCE_FILES})


set(ASSETS
${CMAKE_CURRENT_SOURCE_DIR}/grainflow.record~-help.pd
)


file(COPY ${ASSETS} DESTINATION ${PD_OUTPUT_PATH})

