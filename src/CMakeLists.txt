include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${TDE_INCLUDE_DIR}
  ${TQT_INCLUDE_DIRS}
  ${DBUS_TQT_INCLUDE_DIRS}
  ${DBUS_INCLUDE_DIRS}
)

link_directories(
  ${TQT_LIBRARY_DIRS}
  ${TDE_LIB_DIR}
  ${DBUS_TQT_LIBRARY_DIRS}
  ${DBUS_LIBRARY_DIRS}
)


##### firewallcommon (static) - shared by the kcm and the standalone app

tde_add_library( firewallcommon STATIC_PIC AUTOMOC
  SOURCES
    firewalldclient.cpp
    firewallwidget.cpp
    statuspage.cpp
    zonespage.cpp
    servicespage.cpp
    interfacespage.cpp
    dialogs.cpp
  LINK
    tdeui-shared
    tdecore-shared
    ${DBUS_TQT_LIBRARIES}
    ${DBUS_LIBRARIES}
)


##### kcm_firewalld (kpart)

tde_add_kpart( kcm_firewalld AUTOMOC
  SOURCES
    kcm_firewalld.cpp
  LINK
    firewallcommon-static
    tdeui-shared
    tdecore-shared
  DESTINATION ${PLUGIN_INSTALL_DIR}
)


##### tde-firewalld (standalone application)

tde_add_executable( tde-firewalld AUTOMOC
  SOURCES
    main.cpp
    mainwindow.cpp
  LINK
    firewallcommon-static
    tdeui-shared
    tdecore-shared
  DESTINATION ${BIN_INSTALL_DIR}
)


##### tde-firewalld-tray (system tray icon for kicker)

tde_add_executable( tde-firewalld-tray AUTOMOC
  SOURCES
    traymain.cpp
    firewalltray.cpp
  LINK
    firewallcommon-static
    tdeio-shared
    tdeui-shared
    tdecore-shared
  DESTINATION ${BIN_INSTALL_DIR}
)


##### desktop files

tde_create_translated_desktop(
  SOURCE firewalld.desktop
  DESTINATION ${XDG_APPS_INSTALL_DIR}
)

tde_create_translated_desktop(
  SOURCE tde-firewalld.desktop
  DESTINATION ${XDG_APPS_INSTALL_DIR}
)

tde_create_translated_desktop(
  SOURCE tde-firewalld-tray.desktop
  DESTINATION ${XDG_APPS_INSTALL_DIR}
)

# not started at login unless the user enables it (see the
# X-TDE-autostart-condition in the file)
tde_create_translated_desktop(
  SOURCE tde-firewalld-tray-autostart.desktop
  DESTINATION ${AUTOSTART_INSTALL_DIR}
)
