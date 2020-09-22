def Settings( **kwargs ):
    flags = [ '-x', '-Wall', '-Wextra', '-Werror' ]
    flags = [ '-x', '-Wall', '-Wsystem-headers', '-lrt' ]
    return { 'flags': flags }
