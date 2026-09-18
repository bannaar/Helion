include(GNUInstallDirs)
if(HELION_BUILD_SERVER)
  install(PROGRAMS scripts/helion-dev-cert DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(HELION_BUILD_CLIENT AND HELION_BUILD_SERVER)
  install(PROGRAMS scripts/helion-play DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES assets/helion.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES assets/helion.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()
install(FILES README.md ${PROJECT_SOURCE_DIR}/docs/native-install.md
  ${PROJECT_SOURCE_DIR}/docs/native-server.md ${PROJECT_SOURCE_DIR}/docs/native-client.md
  ${PROJECT_SOURCE_DIR}/docs/PROJECT_STATE.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/helion)
