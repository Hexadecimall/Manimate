function(manimation_add_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Manimation::Core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
endfunction()

manimation_add_test(tst_document)

qt_add_executable(tst_launcher tst_launcher.cpp)
target_link_libraries(tst_launcher PRIVATE Manimation::Ui Qt6::Test)
add_test(NAME tst_launcher COMMAND tst_launcher)
set_target_properties(tst_launcher PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
