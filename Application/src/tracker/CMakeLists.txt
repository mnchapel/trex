add_subdirectory(gui)
if(NOT COMMONS_NO_PYTHON)
    add_subdirectory(python)
endif()
add_subdirectory(tracking)
add_subdirectory(misc)

include_directories(${CMAKE_SOURCE_DIR}/src/ProcessedVideo)

if(APPLE)
    set(TREX_EXEC_NAME TRex)
else()
    set(TREX_EXEC_NAME trex)
endif()

# to include generated moc headers, since they are generated in binary dir
set(CMAKE_INCLUDE_CURRENT_DIR ON)

file(GLOB SRCS
    gui.h
    gui.cpp
    main.cpp
    VideoOpener.cpp
    VideoOpener.h
    ../grabber/default_config.cpp
    ../grabber/default_config.h
    gfx/TRexIcon16.png
    gfx/TRexIcon32.png
    gfx/TRexIcon64.png
    "${CMAKE_CURRENT_BINARY_DIR}/../GitSHA1.cpp" ../GitSHA1.h
)
 
if(APPLE)
 set(SRCS ${SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/gfx/TRexIcon.icns)
endif()

if(WIN32 OR UNIX AND NOT APPLE)
	set(SRCS ${SRCS} trex.rc)
endif()

if(NOT WIN32 AND TREX_WITH_TESTS)
    #windows does not support large cpp files
    add_subdirectory(tests)
    
    if(APPLE)
        add_executable(test_imgui program.cpp)
    else()
        add_executable(test_imgui program.cpp)
    endif(APPLE)

    target_link_libraries(test_imgui PUBLIC
        framegrabber_misc
        tracker_tracking
        commons
    )
    #target_precompile_headers(test_imgui REUSE_FROM commons)
    add_dependencies(test_imgui commons) #Commons::LibZIP Commons::GLFW Commons::OpenCV)
endif()

add_executable(properties properties.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/../GitSHA1.cpp" ../GitSHA1.h)

target_link_libraries(properties 
    PUBLIC 
        tracker_gui
        tracker_tracking
        tracker_misc
        commons 
        pv)

if(NOT COMMONS_NO_PYTHON AND NOT TREX_NO_PYTHON)
    target_link_libraries(properties 
        PUBLIC tracker_python)
endif()

# add actual executable
add_executable(${TREX_EXEC_NAME} ${SRCS})
add_dependencies(${TREX_EXEC_NAME} commons Commons::GLFW
    Commons::OpenCV
    Commons::FFMPEG)
#Commons::GLFW Commons::OpenCV)

# link all needed libraries to the target
target_link_libraries(${TREX_EXEC_NAME} PUBLIC
	tracker_tracking
	tracker_gui
    tracker_misc
	pv
	commons
)

if(NOT ${COMMONS_NO_PYTHON} AND NOT ${TREX_NO_PYTHON})
    target_link_libraries(${TREX_EXEC_NAME} 
        PUBLIC tracker_python)
    message(STATUS "Linking tracker_python")
endif()

if(WITH_HTTPD)
    add_dependencies(${TREX_EXEC_NAME} LibMicroHttpd)
endif()

if(WIN32 AND NOT COMMONS_NO_PYTHON)
	set_target_properties(${TREX_EXEC_NAME} PROPERTIES LINK_FLAGS "/DELAYLOAD:tracker_python.dll /DELAYLOAD:tracker_python-d.dll")
endif()

if(NOT WIN32)
    target_link_libraries(${TREX_EXEC_NAME} PUBLIC pthread m)
    if(TREX_WITH_TESTS)
        target_link_libraries(test_imgui PUBLIC pthread m)
    endif()
endif()

set_target_properties(${TREX_EXEC_NAME} PROPERTIES
  MACOSX_BUNDLE TRUE
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.trex.run"
  MACOSX_BUNDLE_BUNDLE_NAME TRex
  MACOSX_BUNDLE_INFO_STRING "Multi-species interactive video-based tracking"
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
)

set(TRACKER_PATH "$<TARGET_FILE_DIR:${TREX_EXEC_NAME}>")
message("TRACKER PATH ${TRACKER_PATH}")

if(NOT APPLE)
    if(NOT COMMONS_NO_PYTHON)
        set_target_properties(trex_check_python PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TRACKER_PATH}")
    endif()

    if(TREX_WITH_TESTS)
        set_target_properties(test_imgui PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TRACKER_PATH}")
    endif()
endif()

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/gfx/TRexIcon.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

set(RESOURCE_COPY_FILES
	${CMAKE_SOURCE_DIR}/src/html
	${CMAKE_SOURCE_DIR}/src/fonts
    ${CMAKE_CURRENT_SOURCE_DIR}/gfx
)
set(RESOURCE_COPY_SINGLE_FILES
	${CMAKE_SOURCE_DIR}/default.settings
    ${CMAKE_SOURCE_DIR}/learn_static.py
    ${CMAKE_CURRENT_SOURCE_DIR}/python/trex_init.py
    ${CMAKE_CURRENT_SOURCE_DIR}/python/trex_learn_category.py
)

IF("${CMAKE_SYSTEM}" MATCHES "Linux")
	message(STATUS "Copying files ${RESOURCE_COPY_SINGLE_FILES}")
	foreach(comp ${RESOURCE_COPY_SINGLE_FILES})
        get_filename_component(comp_s ${comp} NAME)

		add_custom_command(TARGET ${TREX_EXEC_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                "${comp}"
                "$<TARGET_FILE_DIR:${TREX_EXEC_NAME}>/${comp_s}")
		message(STATUS ${CMAKE_COMMAND} -E create_symlink
            "${comp}"
            "$<TARGET_FILE_DIR:${TREX_EXEC_NAME}>/${comp_s}")
	endforeach()

	foreach(comp ${RESOURCE_COPY_FILES})
        get_filename_component(comp_s ${comp} NAME)

		add_custom_command(TARGET ${TREX_EXEC_NAME} POST_BUILD
		    COMMAND ${CMAKE_COMMAND} -E copy_directory
		        "${comp}"
		        "$<TARGET_FILE_DIR:${TREX_EXEC_NAME}>/${comp_s}")
	endforeach()
elseif(WIN32)
    message(STATUS "(windows) Copying files ${RESOURCE_COPY_SINGLE_FILES}")
    message(STATUS "(install_prefix) :${CMAKE_INSTALL_PREFIX} :${CMAKE_BINARY_DIR}")
    
    foreach(comp ${RESOURCE_COPY_SINGLE_FILES})
        get_filename_component(comp_s ${comp} NAME)

        add_custom_command(TARGET ${TREX_EXEC_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "${comp}"
                "$<TARGET_FILE_DIR:${TREX_EXEC_NAME}>/${comp_s}")
    endforeach()

    message(STATUS "(windows) Copying folders ${RESOURCE_COPY_FILES}")
    foreach(comp ${RESOURCE_COPY_FILES})
        get_filename_component(comp_s ${comp} NAME)

        add_custom_command(TARGET ${TREX_EXEC_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${comp}"
                "$<TARGET_FILE_DIR:${TREX_EXEC_NAME}>/${comp_s}")
    endforeach()
    
elseif(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
	message(STATUS "Copying files ${RESOURCE_COPY_SINGLE_FILES}")
    foreach(comp ${RESOURCE_COPY_SINGLE_FILES})
        get_filename_component(comp_s ${comp} NAME)
        add_custom_command(TARGET ${TREX_EXEC_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                        "${comp}"
                        "$<TARGET_FILE_DIR:${TREX_EXEC_NAME}>/../Resources/${comp_s}")
        install(FILES ${comp} DESTINATION bin)
    endforeach()

	foreach(comp ${RESOURCE_COPY_FILES})
        get_filename_component(comp_s ${comp} NAME)
		add_custom_command(TARGET ${TREX_EXEC_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${comp}"
                "$<TARGET_FILE_DIR:${TREX_EXEC_NAME}>/../Resources/${comp_s}")
	endforeach()
endif()

set(targets ${TREX_EXEC_NAME})
if(WIN32 AND NOT COMMONS_NO_PYTHON)
set(targets ${targets} tracker_python)
endif()

if(APPLE)
add_executable(trex_starter trex.cpp)
target_link_libraries(trex_starter PRIVATE commons)
set_target_properties(trex_starter PROPERTIES OUTPUT_NAME trex)
set(targets ${targets} trex_starter)

#target_precompile_headers(trex_starter REUSE_FROM commons)
endif()

if(APPLE)
target_sources(${TREX_EXEC_NAME} PRIVATE gfx/TRexIcons.xcassets)
set_source_files_properties(gfx/TRexIcons.xcassets PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources
)
set_target_properties(${TREX_EXEC_NAME} PROPERTIES
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
endif()

#target_precompile_headers(properties REUSE_FROM commons)
#target_precompile_headers(${TREX_EXEC_NAME} REUSE_FROM commons)

install(TARGETS ${targets}
    EXPORT TRex
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION bin
    INCLUDES DESTINATION include
    CONFIGURATIONS Release
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/fonts DESTINATION ${CMAKE_INSTALL_PREFIX}/usr/share/trex)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/html DESTINATION ${CMAKE_INSTALL_PREFIX}/usr/share/trex)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gfx DESTINATION ${CMAKE_INSTALL_PREFIX}/usr/share/trex)
install(FILES ${CMAKE_SOURCE_DIR}/default.settings ${CMAKE_SOURCE_DIR}/learn_static.py ${CMAKE_CURRENT_SOURCE_DIR}/python/trex_init.py ${CMAKE_CURRENT_SOURCE_DIR}/python/trex_learn_category.py DESTINATION ${CMAKE_INSTALL_PREFIX}/usr/share/trex)
