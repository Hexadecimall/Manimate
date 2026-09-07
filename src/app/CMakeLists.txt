qt_add_executable(manimation MACOSX_BUNDLE main.cpp)

target_link_libraries(manimation PRIVATE Manimation::Ui)

set_target_properties(manimation PROPERTIES
    OUTPUT_NAME Manimation
    MACOSX_BUNDLE_BUNDLE_NAME Manimation
    MACOSX_BUNDLE_GUI_IDENTIFIER app.manimation.editor
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    WIN32_EXECUTABLE TRUE
)
