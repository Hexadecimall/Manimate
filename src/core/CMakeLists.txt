configure_file(Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/Version.h @ONLY)

add_library(manimation_core STATIC
    Json.cpp
    SceneObject.cpp
    Timeline.cpp
    Document.cpp
    Project.cpp
    RecentProjects.cpp
)

add_library(Manimation::Core ALIAS manimation_core)

target_include_directories(manimation_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Core deliberately stops at Gui (for QColor) and never links Widgets, so the
# document model stays testable without a window system.
target_link_libraries(manimation_core PUBLIC Qt6::Core Qt6::Gui)
