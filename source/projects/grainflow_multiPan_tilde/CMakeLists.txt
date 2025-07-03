PROJECT("grainflow_multiPan_tilde")


include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../GrainflowLib/include")

set(
    SOURCE_FILES
	${PROJECT_NAME}.cpp
)

ADD_PD_EXTERNAL(
 ${PROJECT_NAME}
 grainflow.multiPan~
 ${SOURCE_FILES})

 set(ASSETS
 ${CMAKE_CURRENT_SOURCE_DIR}/grainflow.multiPan~-help.pd
 )

file(COPY ${ASSETS} DESTINATION ${PD_OUTPUT_PATH})