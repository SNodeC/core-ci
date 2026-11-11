set(CPACK_PACKAGE_NAME "core-ci")
set(CPACK_PACKAGE_VENDOR "SNodeC")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "Volker Christian <me@vchrist.at>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SNode.C package build orchestrator")
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_STRIP_FILES YES)
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/_packages")
set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libmariadb3, systemd")
set(CPACK_RPM_PACKAGE_REQUIRES "mariadb-connector-c, openssl-libs, systemd")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/postrm"
)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/postinst"
)
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/postrm"
)
set(CPACK_RPM_PACKAGE_RELEASE 1)
set(CPACK_RPM_PACKAGE_RELEASE_DIST OFF)
set(CPACK_RPM_PACKAGE_RELOCATABLE OFF)
set(CPACK_SOURCE_IGNORE_FILES
    "/build[^/]*/;/checkouts/;/.git/;/.qtcreator/;/.local-credentials/;/secrets/;~$"
)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/PackageConfig.cmake")
if(CORECI_WITH_SNODEC)
    configure_file(packaging/core-ci.service.in coreci.service @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/coreci.service"
            DESTINATION lib/systemd/system
    )
    install(
        FILES packaging/core-ci.sysusers.conf
        DESTINATION lib/sysusers.d
        RENAME core-ci.conf
    )
    install(FILES packaging/core-ci.env.example packaging/coreci.conf.example
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/core-ci
    )
    include(CPack)
endif()
