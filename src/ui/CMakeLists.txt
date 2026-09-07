add_library(manimation_ui STATIC
    theme/Theme.cpp
    widgets/Wordmark.cpp
    widgets/WindowButton.cpp
    widgets/TitleBar.cpp
    widgets/AppWindow.cpp
    launcher/RecentProjectsModel.cpp
    launcher/ProjectCardDelegate.cpp
    launcher/NewProjectDialog.cpp
    launcher/LauncherWindow.cpp
    code/ManimVocabulary.cpp
    code/PythonHighlighter.cpp
    code/CodeEditor.cpp
    code/CodeEditorWindow.cpp
)

add_library(Manimation::Ui ALIAS manimation_ui)

target_include_directories(manimation_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/theme
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/launcher
    ${CMAKE_CURRENT_SOURCE_DIR}/code
)

target_link_libraries(manimation_ui PUBLIC Manimation::Core Qt6::Widgets)
