###########################################
#                                         #
#  This file is released under GPL >= 2   #
#                                         #
###########################################


# required stuff
find_package( TQt )
find_package( TDE )

tde_setup_compiler_flags( )
tde_setup_largefiles( )


##### check for gcc visibility support

if( WITH_GCC_VISIBILITY )
  tde_setup_gcc_visibility( )
endif( WITH_GCC_VISIBILITY )


##### check for dbus-1-tqt (TQt bindings for D-Bus)

pkg_search_module( DBUS_TQT dbus-1-tqt )
if( NOT DBUS_TQT_FOUND )
  tde_message_fatal( "dbus-1-tqt is required but was not found on your system" )
endif( )


##### check for libdbus-1 (needed for interactive-authorization flag on systemd calls)

pkg_search_module( DBUS dbus-1 )
if( NOT DBUS_FOUND )
  tde_message_fatal( "dbus-1 is required but was not found on your system" )
endif( )
