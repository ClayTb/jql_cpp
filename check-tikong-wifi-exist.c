#include <gio/gio.h>
#include <string.h>
#include <nm-dbus-interface.h>
#include <NetworkManager.h>
/*
gcc -Wall list-tikong-conf.c -o list-tikong-conf `pkg-config --cflags --libs libnm`
检查所有连接，看有没有tikong-wifi的名字
*/


static void
get_active_connection_details (const char *obj_path)
{
	GDBusProxy *props_proxy;
	GVariant *ret = NULL, *path_value = NULL;
	const char *path = NULL;
	GError *error = NULL;

	/* This function gets the backing Connection object that describes the
	 * network configuration that the ActiveConnection object is actually using.
	 * The ActiveConnection object contains the mapping between the configuration
	 * and the actual network interfaces that are using that configuration.
	 */

	/* Create a D-Bus object proxy for the active connection object's properties */
	props_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
	                                             G_DBUS_PROXY_FLAGS_NONE,
	                                             NULL,
	                                             NM_DBUS_SERVICE,
	                                             obj_path,
	 "org.freedesktop.NetworkManager.Settings.Connection",
	                                             NULL, NULL);
	g_assert (props_proxy);

	/* Get the object path of the Connection details */
	ret = g_dbus_proxy_call_sync (props_proxy,
	                              "GetSettings",
	                              NULL,
	                              G_DBUS_CALL_FLAGS_NONE, -1,
	                              NULL, &error);
	if (!ret) {
		g_dbus_error_strip_remote_error (error);
		g_warning ("Failed to get active connection Connection property: %s\n",
		           error->message);
		g_error_free (error);
		goto out;
	}

	g_variant_get (ret, "(@a{sa{sv}})", &path_value);

	GVariant *s_con = NULL;
	gboolean found;
	const char *id, *type;
	s_con = g_variant_lookup_value (path_value, NM_SETTING_CONNECTION_SETTING_NAME, NULL);
	g_assert (s_con != NULL);
	found = g_variant_lookup (s_con, NM_SETTING_CONNECTION_ID, "&s", &id);
	g_assert (found);
	found = g_variant_lookup (s_con, NM_SETTING_CONNECTION_TYPE, "&s", &type);
	g_assert (found);

	/* Dump the configuration to stdout */
	if(strstr(id, "tikong-wifi") != NULL)
	{
		g_print ("%s <=> %s\n", id, obj_path);
		g_print ("%s <=> %s\n", id, type);
	}

	/* Connection setting first */
	//print_setting (NM_SETTING_CONNECTION_SETTING_NAME, s_con);

	//path = g_variant_get_string (path_value, NULL);

	/* Print out the actual connection details */
	//g_print(path);

out:
	if (path_value)
		g_variant_unref (path_value);
	if (ret)
		g_variant_unref (ret);
	g_object_unref (props_proxy);
}



static void
list_connections (GDBusProxy *proxy)
{
	int i;
	GError *error = NULL;
	GVariant *ret;
	char **paths;

	/* Call ListConnections D-Bus method */
	ret = g_dbus_proxy_call_sync (proxy,
	                              "ListConnections",
	                              NULL,
	                              G_DBUS_CALL_FLAGS_NONE, -1,
	                              NULL, &error);
	if (!ret) {
		g_dbus_error_strip_remote_error (error);
		g_print ("ListConnections failed: %s\n", error->message);
		g_error_free (error);
		return;
	}

	g_variant_get (ret, "(^ao)", &paths);
	g_variant_unref (ret);

	for (i = 0; paths[i]; i++)
    {
		//g_print ("%s\n", paths[i]);
    //    if(strstr(paths[i], "7") != NULL )
        {
            //g_print("%s\n", paths[i]);
            //GVariant *value = NULL;
            //返回的是Variant，这里取出来
            //g_variant_get (paths[i], "(v)", &value);
            get_active_connection_details (paths[i]);

            /* And print out the details for each active connection */
           // for (i = 0; paths1[i]; i++) {
           //     g_print ("Active connection path: %s\n", paths1[i]);
           //     get_active_connection_details (paths1[i]);
           // }
          //  g_strfreev (paths1);
        }
    }
	g_strfreev (paths);
}

int
main (int argc, char *argv[])
{
	GDBusProxy *proxy;

	/* Create a D-Bus proxy; NM_DBUS_* defined in nm-dbus-interface.h */
	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
	                                       G_DBUS_PROXY_FLAGS_NONE,
	                                       NULL,
	                                       NM_DBUS_SERVICE,
	                                       NM_DBUS_PATH_SETTINGS,
	                                       NM_DBUS_INTERFACE_SETTINGS,
	                                       NULL, NULL);
	g_assert (proxy != NULL);

	/* List connections of system settings service */
	list_connections (proxy);

	g_object_unref (proxy);

	return 0;
}
