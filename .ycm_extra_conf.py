def Settings( **kwargs ):
    flags = [ '-x', '-Wall', '-Wextra', '-Werror' ]
    flags = [ '-x', '-Wsystem-headers', '-std=c99' ]
    return { 'flags': flags }
