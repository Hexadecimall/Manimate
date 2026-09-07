configure_file(Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/Version.h @ONLY)

add_library(manimate_core STATIC
    Json.cpp
    SceneObject.cpp
    Timeline.cpp
    Document.cpp
    Project.cpp
    PythonImport.cpp
    SceneTemplate.cpp
    Catalog.cpp
    RateFunctions.cpp
    SceneEvaluator.cpp
    CodeGenerator.cpp
    CodeParser.cpp
    RecentProjects.cpp
)

add_library(Manimate::Core ALIAS manimate_core)

target_include_directories(manimate_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Core deliberately stops at Gui (for QColor) and never links Widgets, so the
# document model stays testable without a window system.
target_link_libraries(manimate_core PUBLIC Qt6::Core Qt6::Gui)
