function(load_dotenv DOTENV_FILE)
    if(NOT EXISTS "${DOTENV_FILE}")
        message(FATAL_ERROR ".env file not found: ${DOTENV_FILE}")
    endif()

    file(STRINGS "${DOTENV_FILE}" ENV_LINES)

    foreach(line IN LISTS ENV_LINES)
        string(STRIP "${line}" line)

        if(line MATCHES "^[ \t]*$" OR line MATCHES "^[ \t]*#")
            continue()
        endif()

        if(line MATCHES "^([^=]+)=(.*)$")
            set(key "${CMAKE_MATCH_1}")
            set(value "${CMAKE_MATCH_2}")

            string(STRIP "${key}" key)
            string(STRIP "${value}" value)

            if(value MATCHES "^\"(.*)\"$")
                set(value "${CMAKE_MATCH_1}")
            elseif(value MATCHES "^'(.*)'$")
                set(value "${CMAKE_MATCH_1}")
            endif()

            set(${key} "${value}" PARENT_SCOPE)
        endif()
    endforeach()
endfunction()