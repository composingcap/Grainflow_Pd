PROJECT("grainflow_tilde")


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
 grainflow~
 ${SOURCE_FILES})

set(ASSETS
${CMAKE_CURRENT_SOURCE_DIR}/grainflow~-help.pd
${CMAKE_CURRENT_SOURCE_DIR}/mc_fm.pd )

 file(COPY ${ASSETS} DESTINATION ${PD_OUTPUT_PATH})

# set(RELFLAGS )
# if (MSVC)
# set(RELFLAGS /Oi /O2 /Ot /Ob2)
# endif()
# if(APPLE)
# set(RELFLAGS -Ofast)
# endif()
# target_compile_options(${PROJECT_NAME} PRIVATE
# "$<$<CONFIG:Release>:${RELFLAGS}>"
# )