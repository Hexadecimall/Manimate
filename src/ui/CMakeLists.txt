add_library(manimate_ui STATIC
    theme/Theme.cpp
    widgets/Wordmark.cpp
    widgets/WindowButton.cpp
    widgets/TitleBar.cpp
    widgets/AppWindow.cpp
    widgets/SegmentedTabs.cpp
    launcher/RecentProjectsModel.cpp
    launcher/ProjectCardDelegate.cpp
    launcher/NewProjectDialog.cpp
    launcher/LauncherWindow.cpp
    code/ManimVocabulary.cpp
    code/PythonHighlighter.cpp
    code/CodeEditor.cpp
    code/CodeEditorWindow.cpp
    render/SceneRenderer.cpp
    editor/EditorState.cpp
    editor/CanvasView.cpp
    editor/TimelineView.cpp
    editor/LibraryPanel.cpp
    editor/InspectorPanel.cpp
    editor/SceneOutliner.cpp
    audio/AudioPlayer.cpp
    export/RenderJob.cpp
    project/ProjectWindow.cpp
)

add_library(Manimate::Ui ALIAS manimate_ui)

target_include_directories(manimate_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/theme
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/launcher
    ${CMAKE_CURRENT_SOURCE_DIR}/code
    ${CMAKE_CURRENT_SOURCE_DIR}/project
    ${CMAKE_CURRENT_SOURCE_DIR}/render
    ${CMAKE_CURRENT_SOURCE_DIR}/editor
    ${CMAKE_CURRENT_SOURCE_DIR}/export
    ${CMAKE_CURRENT_SOURCE_DIR}/audio
)

# The only Objective-C++ in the project: macOS draws a window shadow of its
# own, and it has to be told not to.
if(APPLE)
    target_sources(manimate_ui PRIVATE widgets/MacWindow.mm)
    target_link_libraries(manimate_ui PRIVATE "-framework AppKit")
endif()

target_link_libraries(manimate_ui PUBLIC Manimate::Core Qt6::Widgets Qt6::Multimedia)
