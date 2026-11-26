set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_LIST_DIR}/postinst;${CMAKE_CURRENT_LIST_DIR}/postrm"
)
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_LIST_DIR}/postinst")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_LIST_DIR}/postrm")
if(DEFINED ENV{DEPENDENCIES_JSON})
    set(dependencies "$ENV{DEPENDENCIES_JSON}")
    string(JSON count LENGTH "${dependencies}")
    if(NOT count EQUAL 2)
        message(
            FATAL_ERROR "Expected resolved snodec and snodec-idp dependencies"
        )
    endif()
    if(CPACK_GENERATOR STREQUAL "DEB")
        string(JSON suffix GET "$ENV{TARGET_JSON}" codename)
        set(suffix "~${suffix}")
        set(variable CPACK_DEBIAN_PACKAGE_DEPENDS)
    else()
        string(JSON suffix GET "$ENV{TARGET_JSON}" disttag)
        set(suffix ".${suffix}")
        set(variable CPACK_RPM_PACKAGE_REQUIRES)
    endif()
    foreach(index RANGE 0 1)
        string(JSON package GET "${dependencies}" ${index} package)
        string(JSON version GET "${dependencies}" ${index} version)
        string(JSON revision GET "${dependencies}" ${index} revision)
        set(upper "")
        if(package STREQUAL "snodec")
            set(names
                snodec-db-mariadb snodec-http-client snodec-net-in-stream-tls
                snodec-http-server-express-tls-in
            )
        elseif(package STREQUAL "snodec-idp")
            set(names snodec-idp-client-dev)
            set(upper "2.1")
        else()
            message(FATAL_ERROR "Unexpected resolved dependency: ${package}")
        endif()
        foreach(name IN LISTS names)
            if(CPACK_GENERATOR STREQUAL "DEB")
                string(APPEND ${variable}
                       ", ${name} (>= ${version}-${revision}${suffix})"
                )
                if(upper)
                    string(APPEND ${variable} ", ${name} (<< ${upper})")
                endif()
            else()
                string(APPEND ${variable}
                       ", ${name} >= ${version}-${revision}${suffix}"
                )
                if(upper)
                    string(APPEND ${variable} ", ${name} < ${upper}")
                endif()
            endif()
        endforeach()
    endforeach()
endif()
file(MAKE_DIRECTORY "${CPACK_OUTPUT_FILE_PREFIX}")
file(WRITE "${CPACK_OUTPUT_FILE_PREFIX}/core-ci.packages"
     "${CPACK_PACKAGE_NAME}\n"
)
