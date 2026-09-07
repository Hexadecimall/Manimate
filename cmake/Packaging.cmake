# Installation and packaging.
#
# Each platform gets the package its users expect: a .dmg on macOS, an NSIS
# installer plus a portable .zip on Windows, and a relocatable tarball on Linux
# that the release workflow also turns into an AppImage. Qt is deployed into
# every one of them, so nothing has to be installed alongside the application.

include(GNUInstallDirs)

# Names used for release assets, e.g. Manimation-0.1.0-macos-arm64.dmg
if(APPLE)
    set(MANIMATION_PLATFORM macos)
elseif(WIN32)
    set(MANIMATION_PLATFORM windows)
else()
    set(MANIMATION_PLATFORM linux)
endif()

if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    # A build covering more than one architecture is a universal binary, and
    # that is what the package should be called.
    list(LENGTH CMAKE_OSX_ARCHITECTURES MANIMATION_ARCH_COUNT)
    if(MANIMATION_ARCH_COUNT GREATER 1)
        set(MANIMATION_ARCH universal)
    else()
        set(MANIMATION_ARCH "${CMAKE_OSX_ARCHITECTURES}")
    endif()
else()
    set(MANIMATION_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

string(TOLOWER "${MANIMATION_ARCH}" MANIMATION_ARCH)
if(MANIMATION_ARCH MATCHES "^(amd64|x64|x86_64)$")
    set(MANIMATION_ARCH x86_64)
elseif(MANIMATION_ARCH MATCHES "^(aarch64|arm64)$")
    set(MANIMATION_ARCH arm64)
endif()

set(MANIMATION_PACKAGE_BASENAME
    "Manimation-${PROJECT_VERSION}-${MANIMATION_PLATFORM}-${MANIMATION_ARCH}")

# ---------------------------------------------------------------- install ----

if(APPLE)
    install(TARGETS manimation BUNDLE DESTINATION .)
elseif(WIN32)
    install(TARGETS manimation RUNTIME DESTINATION .)
else()
    install(TARGETS manimation RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(FILES ${CMAKE_SOURCE_DIR}/resources/app.manimation.editor.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES ${CMAKE_SOURCE_DIR}/resources/app.manimation.editor.xml
            DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)

    foreach(size 16 32 48 64 128 256 512)
        install(FILES ${CMAKE_SOURCE_DIR}/resources/icon_${size}.png
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
                RENAME app.manimation.editor.png)
    endforeach()
endif()

# Bundle the Qt libraries and plugins the application actually uses.
qt_generate_deploy_app_script(
    TARGET manimation
    OUTPUT_SCRIPT MANIMATION_DEPLOY_SCRIPT
    NO_UNSUPPORTED_PLATFORM_ERROR
    NO_TRANSLATIONS
)
install(SCRIPT ${MANIMATION_DEPLOY_SCRIPT})

# ------------------------------------------------------------------ cpack ----

set(CPACK_PACKAGE_NAME Manimation)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY Manimation)
set(CPACK_PACKAGE_FILE_NAME ${MANIMATION_PACKAGE_BASENAME})
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Hexadecimall/Manimation")
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_STRIP_FILES ON)

if(APPLE)
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_DMG_VOLUME_NAME "Manimation ${PROJECT_VERSION}")
    set(CPACK_DMG_FORMAT UDZO)
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_PACKAGE_NAME "Manimation")
    set(CPACK_NSIS_DISPLAY_NAME "Manimation ${PROJECT_VERSION}")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "Manimation.exe")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/resources/icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/resources/icon.ico")
    set(CPACK_NSIS_URL_INFO_ABOUT "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Manimation.lnk' '$INSTDIR\\\\Manimation.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$START_MENU\\\\Manimation.lnk'")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
