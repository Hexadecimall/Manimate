set(app_sources main.cpp)

if(WIN32)
    list(APPEND app_sources ${CMAKE_SOURCE_DIR}/resources/manimate.rc)
endif()

qt_add_executable(manimate MACOSX_BUNDLE ${app_sources})

target_link_libraries(manimate PRIVATE Manimate::Ui)

set_target_properties(manimate PROPERTIES
    OUTPUT_NAME Manimate
    WIN32_EXECUTABLE TRUE
)

if(APPLE)
    set(icon ${CMAKE_SOURCE_DIR}/resources/icon.icns)
    target_sources(manimate PRIVATE ${icon})
    set_source_files_properties(${icon} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    set_target_properties(manimate PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME Manimate
        MACOSX_BUNDLE_GUI_IDENTIFIER app.manimate.editor
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_ICON_FILE icon.icns
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
    )
endif()

include(${CMAKE_SOURCE_DIR}/cmake/Packaging.cmake)
