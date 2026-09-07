# Installation and packaging.
#
# Every platform gets two packages: the installer its users expect, and a
# ready-to-run archive for anyone who would rather not install anything.
#
#   macOS    .dmg              .app in a .zip
#   Windows  NSIS installer    portable folder in a .zip
#   Linux    .deb             .AppImage (single file, and a .tar.gz)
#
# Qt is deployed into all of them, so nothing has to be installed alongside.

include(GNUInstallDirs)

# Names used for release assets, e.g. Manimate-0.1.0-macos-arm64.dmg
if(APPLE)
    set(MANIMATE_PLATFORM macos)
elseif(WIN32)
    set(MANIMATE_PLATFORM windows)
else()
    set(MANIMATE_PLATFORM linux)
endif()

if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    # A build covering more than one architecture is a universal binary, and
    # that is what the package should be called.
    list(LENGTH CMAKE_OSX_ARCHITECTURES MANIMATE_ARCH_COUNT)
    if(MANIMATE_ARCH_COUNT GREATER 1)
        set(MANIMATE_ARCH universal)
    else()
        set(MANIMATE_ARCH "${CMAKE_OSX_ARCHITECTURES}")
    endif()
else()
    set(MANIMATE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

string(TOLOWER "${MANIMATE_ARCH}" MANIMATE_ARCH)
if(MANIMATE_ARCH MATCHES "^(amd64|x64|x86_64)$")
    set(MANIMATE_ARCH x86_64)
elseif(MANIMATE_ARCH MATCHES "^(aarch64|arm64)$")
    set(MANIMATE_ARCH arm64)
endif()

set(MANIMATE_PACKAGE_BASENAME
    "Manimate-${PROJECT_VERSION}-${MANIMATE_PLATFORM}-${MANIMATE_ARCH}")

# ---------------------------------------------------------------- install ----

if(APPLE)
    install(TARGETS manimate BUNDLE DESTINATION .)
elseif(WIN32)
    install(TARGETS manimate RUNTIME DESTINATION .)
else()
    install(TARGETS manimate RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(FILES ${CMAKE_SOURCE_DIR}/resources/app.manimate.editor.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES ${CMAKE_SOURCE_DIR}/resources/app.manimate.editor.xml
            DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)

    foreach(size 16 32 48 64 128 256 512)
        install(FILES ${CMAKE_SOURCE_DIR}/resources/icon_${size}.png
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
                RENAME app.manimate.editor.png)
    endforeach()
endif()

# Bundle the Qt libraries and plugins the application actually uses.
qt_generate_deploy_app_script(
    TARGET manimate
    OUTPUT_SCRIPT MANIMATE_DEPLOY_SCRIPT
    NO_UNSUPPORTED_PLATFORM_ERROR
    NO_TRANSLATIONS
)
install(SCRIPT ${MANIMATE_DEPLOY_SCRIPT})

# ------------------------------------------------------------------ cpack ----

set(CPACK_PACKAGE_NAME Manimate)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY Manimate)
set(CPACK_PACKAGE_FILE_NAME ${MANIMATE_PACKAGE_BASENAME})
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Hexadecimall/Manimate")
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_STRIP_FILES ON)

if(APPLE)
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_DMG_VOLUME_NAME "Manimate ${PROJECT_VERSION}")
    set(CPACK_DMG_FORMAT UDZO)
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_PACKAGE_NAME "Manimate")
    set(CPACK_NSIS_DISPLAY_NAME "Manimate ${PROJECT_VERSION}")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "Manimate.exe")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/resources/icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/resources/icon.ico")
    set(CPACK_NSIS_URL_INFO_ABOUT "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Manimate.lnk' '$INSTDIR\\\\Manimate.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$START_MENU\\\\Manimate.lnk'")
else()
    set(CPACK_GENERATOR "DEB;TGZ")

    set(CPACK_DEBIAN_PACKAGE_NAME manimate)
    set(CPACK_DEBIAN_PACKAGE_SECTION graphics)
    set(CPACK_DEBIAN_PACKAGE_PRIORITY optional)
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Manimate")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    # Qt travels inside the package, so only the system libraries it cannot
    # carry are declared.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
endif()

include(CPack)
