set(app_sources main.cpp)

if(WIN32)
    list(APPEND app_sources ${CMAKE_SOURCE_DIR}/resources/manimation.rc)
endif()

qt_add_executable(manimation MACOSX_BUNDLE ${app_sources})

target_link_libraries(manimation PRIVATE Manimation::Ui)

set_target_properties(manimation PROPERTIES
    OUTPUT_NAME Manimation
    WIN32_EXECUTABLE TRUE
)

if(APPLE)
    set(icon ${CMAKE_SOURCE_DIR}/resources/icon.icns)
    target_sources(manimation PRIVATE ${icon})
    set_source_files_properties(${icon} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    set_target_properties(manimation PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME Manimation
        MACOSX_BUNDLE_GUI_IDENTIFIER app.manimation.editor
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_ICON_FILE icon.icns
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
    )
endif()

include(${CMAKE_SOURCE_DIR}/cmake/Packaging.cmake)
