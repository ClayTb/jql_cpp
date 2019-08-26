#include <gio/gio.h>
#include <uuid/uuid.h>
#include <string.h>
#include <nm-dbus-interface.h>
#include <NetworkManager.h>
/*
gcc -Wall tkwifi-gdbus.c -o tkwifi-gdbus `pkg-config --cflags --libs libnm uuid`

1. 检查所有连接，看有没有tikong-wifi的名字
2. 创建连接函数 addWifiConnection()
3. 删除连接 remove_connection()
3. 连接wifi函数 connect_wifi()
4. 断开wifi函数 disconn_wifi()
5. 测试网络连通性函数 test_wifi()
*/

char *
nm_utils_uuid_generate (void)
{
	uuid_t uuid;
	char *buf;

	buf = g_malloc0 (37);
	uuid_generate_random (uuid);
	uuid_unparse_lower (uuid, &buf[0]);
	return buf;
}

static void
add_connection (GDBusProxy *proxy, const char *con_name)
{
	GVariantBuilder connection_builder;
	GVariantBuilder setting_builder;
	char *uuid;
	const char *new_con_path;
	GVariant *ret;
	GError *error = NULL;
//创建连接的接口是 AddConnection
/*
The AddConnection() method
AddConnection (IN  a{sa{sv}} connection,
               OUT o         path);
Add new connection and save it to disk. This operation does not start the network connection unless (1) device is idle and able to connect to the network described by the new connection, and (2) the connection is allowed to be started automatically.
IN a{sa{sv}} connection:
Connection settings and properties.
OUT o path:
Object path of the new connection that was just added.

*/
	/* Initialize connection GVariantBuilder */
	g_variant_builder_init (&connection_builder, G_VARIANT_TYPE ("a{sa{sv}}"));

	/* Build up the 'connection' Setting */
	g_variant_builder_init (&setting_builder, G_VARIANT_TYPE ("a{sv}"));

	uuid = nm_utils_uuid_generate ();
	g_variant_builder_add (&setting_builder, "{sv}",
	                       NM_SETTING_CONNECTION_UUID,
	                       g_variant_new_string (uuid));
	g_free (uuid);

	g_variant_builder_add (&setting_builder, "{sv}",
                           //"id"
	                       NM_SETTING_CONNECTION_ID,
	                       g_variant_new_string (con_name));
	g_variant_builder_add (&setting_builder, "{sv}",
                            //"type"
	                       NM_SETTING_CONNECTION_TYPE,
                           //802-11-wireless
	                       g_variant_new_string (NM_SETTING_WIRELESS_SETTING_NAME));
	g_variant_builder_add (&setting_builder, "{sv}",
                            //"interface-name"
	                       NM_SETTING_CONNECTION_INTERFACE_NAME,
	                       g_variant_new_string ("wlp2s0"));  
    //自动连接                        
    g_variant_builder_add (&setting_builder, "{sv}",
                            //"type"
	                       NM_SETTING_CONNECTION_AUTOCONNECT,
	                       g_variant_new_boolean (FALSE)); 
    
	g_variant_builder_add (&connection_builder, "{sa{sv}}",
	                       NM_SETTING_CONNECTION_SETTING_NAME,
	                       &setting_builder);

	/* Add the (empty) 'wireless' Setting */
	g_variant_builder_init (&setting_builder, G_VARIANT_TYPE ("a{sv}"));
    
    GVariantBuilder *builder;
    GVariant *value;

    builder = g_variant_builder_new (G_VARIANT_TYPE ("ay"));
    g_variant_builder_add (builder, "y", 't');
    g_variant_builder_add (builder, "y", 'i');
    g_variant_builder_add (builder, "y", 'k');
    g_variant_builder_add (builder, "y", 'o');
    g_variant_builder_add (builder, "y", 'n');
    g_variant_builder_add (builder, "y", 'g');
    value = g_variant_new ("ay", builder);
    g_variant_builder_unref (builder);

    g_variant_builder_add (&setting_builder, "{sv}",
                           //"ssid" GArray_guchar_ *
	                       NM_SETTING_WIRELESS_SSID,
	                       value);                                                    
	g_variant_builder_add (&connection_builder, "{sa{sv}}",
	                       NM_SETTING_WIRELESS_SETTING_NAME,
	                       &setting_builder);
    /* Build up the 'wifi-security' Setting */
    g_variant_builder_init (&setting_builder, G_VARIANT_TYPE ("a{sv}"));
    g_variant_builder_add (&setting_builder, "{sv}",
	                       NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,
	                       g_variant_new_string ("wpa-psk"));
    g_variant_builder_add (&setting_builder, "{sv}",
	                       NM_SETTING_WIRELESS_SECURITY_PSK,
	                       g_variant_new_string ("tikong-4g"));                       

	g_variant_builder_add (&connection_builder, "{sa{sv}}",
	                       NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
	                       &setting_builder);
    
    
	/* Build up the 'ipv4' Setting */
	g_variant_builder_init (&setting_builder, G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (&setting_builder, "{sv}",
	                       NM_SETTING_IP_CONFIG_METHOD,
	                       g_variant_new_string (NM_SETTING_IP4_CONFIG_METHOD_AUTO));
    g_variant_builder_add (&setting_builder, "{sv}",
	                       NM_SETTING_IP_CONFIG_NEVER_DEFAULT,
	                       g_variant_new_boolean (TRUE));

	g_variant_builder_add (&connection_builder, "{sa{sv}}",
	                       NM_SETTING_IP4_CONFIG_SETTING_NAME,
	                       &setting_builder);

	/* Call AddConnection with the connection dictionary as argument.
	 * (g_variant_new() will consume the floating GVariant returned from
	 * &connection_builder, and g_dbus_proxy_call_sync() will consume the
	 * floating variant returned from g_variant_new(), so no cleanup is needed.
	 */
	ret = g_dbus_proxy_call_sync (proxy,
	                              "AddConnection",
	                              g_variant_new ("(a{sa{sv}})", &connection_builder),
	                              G_DBUS_CALL_FLAGS_NONE, -1,
	                              NULL, &error);
	if (ret) {
		g_variant_get (ret, "(&o)", &new_con_path);
		g_print ("Added: %s\n", new_con_path);
		g_variant_unref (ret);
	} else {
		g_dbus_error_strip_remote_error (error);
		g_print ("Error adding connection: %s\n", error->message);
		g_clear_error (&error);
	}
}

static gboolean
get_active_connection_details (const char *obj_path)
{
	GDBusProxy *props_proxy;
	GVariant *ret = NULL, *path_value = NULL;
	//const char *path = NULL;
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
                                                 //"org.freedesktop.NetworkManager"
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
    
    gboolean foundTk = FALSE;
	/* Dump the configuration to stdout */
	if(strstr(id, "tikong-wifi") != NULL)
	{
		g_print ("%s <=> %s\n", id, obj_path);
		g_print ("%s <=> %s\n", id, type);
        foundTk = TRUE;
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
    return foundTk;
}



static gboolean
find_tk_wifi (GDBusProxy *proxy)
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
		return FALSE;
	}

	g_variant_get (ret, "(^ao)", &paths);
	g_variant_unref (ret);
    gboolean found = FALSE;

	for (i = 0; paths[i]; i++)
    {
		//g_print ("%s\n", paths[i]);
    //    if(strstr(paths[i], "7") != NULL )
        {
            //g_print("%s\n", paths[i]);
            //GVariant *value = NULL;
            //返回的是Variant，这里取出来
            //g_variant_get (paths[i], "(v)", &value);
            found = get_active_connection_details (paths[i]);
            if(found == TRUE)
            {
                goto OUT;
            }

            /* And print out the details for each active connection */
           // for (i = 0; paths1[i]; i++) {
           //     g_print ("Active connection path: %s\n", paths1[i]);
           //     get_active_connection_details (paths1[i]);
           // }
          //  g_strfreev (paths1);
        }
    }
OUT:
	g_strfreev (paths);
    return found;
}

gboolean
checkExist ()
{
	GDBusProxy *proxy;
    gboolean found = FALSE;

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
    //check if exist    
	found = find_tk_wifi (proxy);
    if(found == TRUE)
    {
        g_print("ok, find it\n");
        found = TRUE;
    }
    else
    {
        //创建连接
        g_print("well, not found, create it then\n");
        found = FALSE;
        
    }
    
	g_object_unref (proxy);

	return found;
}

int addWifiConnection()
{
    GDBusProxy *proxy;
	GError *error = NULL;

	/* Create a D-Bus proxy; NM_DBUS_* defined in nm-dbus-interface.h */
	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
	                                       G_DBUS_PROXY_FLAGS_NONE,
	                                       NULL,
	                                       NM_DBUS_SERVICE,
                                           //"/org/freedesktop/NetworkManager/Settings"
	                                       NM_DBUS_PATH_SETTINGS,
                                           // "org.freedesktop.NetworkManager.Settings"
	                                       NM_DBUS_INTERFACE_SETTINGS,
	                                       NULL, &error);
	if (!proxy) {
		g_dbus_error_strip_remote_error (error);
		g_print ("Could not create NetworkManager D-Bus proxy: %s\n", error->message);
		g_error_free (error);
		return 1;
	}

	/* Add a connection */
	add_connection (proxy, "tikong-wifi");

	g_object_unref (proxy);

	return 0;
}

static gboolean
remove(const char *obj_path)
{
    GDBusProxy *props_proxy;
	GVariant *ret = NULL, *path_value = NULL;
	//const char *path = NULL;
	GError *error = NULL;


	props_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
	                                             G_DBUS_PROXY_FLAGS_NONE,
	                                             NULL,
                                                 //"org.freedesktop.NetworkManager"
	                                             NM_DBUS_SERVICE,
	                                             obj_path,
	 "org.freedesktop.NetworkManager.Settings.Connection",
	                                             NULL, NULL);
	g_assert (props_proxy);

	/* Get the object path of the Connection details */
	ret = g_dbus_proxy_call_sync (props_proxy,
	                              "Delete",
	                              NULL,
	                              G_DBUS_CALL_FLAGS_NONE, -1,
	                              NULL, &error);
	if (!ret) {
		g_dbus_error_strip_remote_error (error);
		g_warning ("Failed to get active connection Connection property: %s\n",
		           error->message);
		g_error_free (error);
        g_print("can't remove %s\n", obj_path);
		goto out;
	}
    g_print("remove %s\n", obj_path);


out:
	if (path_value)
		g_variant_unref (path_value);
	if (ret)
		g_variant_unref (ret);
	g_object_unref (props_proxy);
    
    return TRUE;
}


static gboolean
remove_tk_wifi (GDBusProxy *proxy)
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
		return FALSE;
	}

	g_variant_get (ret, "(^ao)", &paths);
	g_variant_unref (ret);
    gboolean found = FALSE;

	for (i = 0; paths[i]; i++)
    {

        found = get_active_connection_details (paths[i]);
        if(found == TRUE)
        {
            remove(paths[i]);
        }       
        
    }
	g_strfreev (paths);
    return found;
}

gboolean
remove_connection()
{
    GDBusProxy *proxy;
    gboolean found = FALSE;

	/* Create a D-Bus proxy; NM_DBUS_* defined in nm-dbus-interface.h */
	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
	                                       G_DBUS_PROXY_FLAGS_NONE,
	                                       NULL,
	                                       NM_DBUS_SERVICE,
	                                       NM_DBUS_PATH_SETTINGS,
	                                       NM_DBUS_INTERFACE_SETTINGS,
	                                       NULL, NULL);
	g_assert (proxy != NULL);
    
    remove_tk_wifi (proxy);
    
	g_object_unref (proxy);

	return found;
}
static gboolean
connect_tk_wifi (GDBusProxy *proxy)
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
		return FALSE;
	}

	g_variant_get (ret, "(^ao)", &paths);
	g_variant_unref (ret);
    gboolean found = FALSE;

	for (i = 0; paths[i]; i++)
    {

        found = get_active_connection_details (paths[i]);
        if(found == TRUE)
        {
            remove(paths[i]);
        }       
        
    }
	g_strfreev (paths);
    return found;
}
static gboolean
find_all_devices(GDBusProxy *proxy)
{
	int i;
	GError *error = NULL;
	GVariant *ret;
	char **paths;
	gboolean found = FALSE;

	/* Call ListConnections D-Bus method */
	ret = g_dbus_proxy_call_sync (proxy,
	                              "Get",
								//g_variant_new ("(a{sa{sv}})", &connection_builder),
	                              g_variant_new ("(ss)", "org.freedesktop.NetworkManager","AllDevices"),
	                              //g_variant_new ("(ss)", "org.freedesktop.NetworkManager","NetworkingEnabled"),
	                              //g_variant_new ("(ss)", "org.freedesktop.NetworkManager","Version"),
	                              G_DBUS_CALL_FLAGS_NONE, -1,
	                              NULL, &error);
	if (!ret) {
		g_dbus_error_strip_remote_error (error);
		g_print ("ListConnections failed: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}
    //g_print("%s\n", ret);
	//const char *value = NULL;
	//gboolean enabled = FALSE;
	//GVariant *version_value;
	//const char *version = NULL;
	//g_variant_get (ret, "(v)", &version_value);
	//g_variant_unref (ret);
	//version = g_variant_get_string (version_value, NULL);
	
	//g_print(version); 
	GVariant *device_value;
	g_variant_get(ret, "(v)", &device_value);
	g_variant_unref(ret);
	g_variant_get (device_value, "(ao)", paths);


    

	/*for (i = 0; paths[i]; i++)
    {

        g_print("%s\n", paths[i]);   
        
    }
	g_strfreev (paths);*/
    return found;
}

static gboolean
connect_wifi()
{
    GDBusProxy *proxy;
    gboolean found = FALSE;

	/* Create a D-Bus proxy; NM_DBUS_* defined in nm-dbus-interface.h */
	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
	                                       G_DBUS_PROXY_FLAGS_NONE,
	                                       NULL,
                                           // "org.freedesktop.NetworkManager"
	                                       NM_DBUS_SERVICE,
                                           //"/org/freedesktop/NetworkManager"
	                                       NM_DBUS_PATH,
                                           //"org.freedesktop.NetworkManager"
	                                       //NM_DBUS_INTERFACE,
											//"org.freedesktop.DBus.Properties"
	                                       "org.freedesktop.DBus.Properties",
	                                       NULL, NULL);
	g_assert (proxy != NULL);
    
    //connect_tk_wifi (proxy);
	find_all_devices(proxy);
    
	g_object_unref (proxy);

    return TRUE;
}    
int
main (int argc, char *argv[])
{
    gboolean found = FALSE;
    found = checkExist();
    if(found == TRUE)
    {
        //remove_connection();        
    }
    else if(found == FALSE)
    {
        addWifiConnection();
    }
    //enable wifi connection
    connect_wifi();
    return 0;
}
