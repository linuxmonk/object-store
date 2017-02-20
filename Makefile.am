AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib \
	-I$(top_srcdir)/src/lib-mail \
	-I$(top_srcdir)/src/lib-index \
	-I$(top_srcdir)/src/lib-storage \
	-I$(top_srcdir)/src/lib-imap \
	-I$(top_srcdir)/src/plugins/mail-log

NOPLUGIN_LDFLAGS =
lib23_object_store_plugin_la_LDFLAGS = -module -avoid-version

module_LTLIBRARIES = \
	lib23_object_store_plugin.la

lib23_object_store_plugin_la_SOURCES = \
	object-store.c
