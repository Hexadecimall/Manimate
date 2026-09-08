function(manimate_add_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Manimate::Core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
endfunction()

manimate_add_test(tst_document)

qt_add_executable(tst_launcher tst_launcher.cpp)
target_link_libraries(tst_launcher PRIVATE Manimate::Ui Qt6::Test)
add_test(NAME tst_launcher COMMAND tst_launcher)
set_target_properties(tst_launcher PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)

qt_add_executable(tst_timeline tst_timeline.cpp)
target_link_libraries(tst_timeline PRIVATE Manimate::Ui Qt6::Test)
add_test(NAME tst_timeline COMMAND tst_timeline)
set_target_properties(tst_timeline PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)

qt_add_executable(tst_codeeditor tst_codeeditor.cpp)
target_link_libraries(tst_codeeditor PRIVATE Manimate::Ui Qt6::Test)
add_test(NAME tst_codeeditor COMMAND tst_codeeditor)
set_target_properties(tst_codeeditor PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
