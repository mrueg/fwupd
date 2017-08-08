/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include "fu-plugin.h"
#include "fu-plugin-vfuncs.h"

static void
fu_plugin_purism_read_cb (const gchar *line, gpointer user_data)
{
	FuPlugin *plugin = FU_PLUGIN (user_data);
	if (g_strcmp0 (line, "Reading flash...") == 0)
		fu_plugin_set_status (plugin, FWUPD_STATUS_DEVICE_VERIFY);
	//FIXME get percentage completion...
	g_debug ("got '%s'", line);
}

static void
fu_plugin_purism_write_cb (const gchar *line, gpointer user_data)
{
	FuPlugin *plugin = FU_PLUGIN (user_data);
	if (g_strcmp0 (line, "Writing flash...") == 0)
		fu_plugin_set_status (plugin, FWUPD_STATUS_DEVICE_WRITE);
	//FIXME get percentage completion...
	g_debug ("got '%s'", line);
}

static FuDevice *
fu_plugin_purism_create_device (FuPlugin *plugin,
				const gchar *guid,
				const gchar *id,
				GError **error)
{
	g_autofree gchar *device_id = g_strdup_printf ("purism-%s", id);
	g_autofree gchar *flashrom_fn = NULL;
	g_autofree gchar *firmware_orig = NULL;
	g_autofree gchar *basename = NULL;
	g_autoptr(FuDevice) dev = fu_device_new ();

	/* we need flashrom from the host system */
	flashrom_fn = g_find_program_in_path ("flashrom");
	if (flashrom_fn == NULL) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_NOT_SUPPORTED,
				     "flashrom is not installed");
		return FALSE;
	}

	/* if the original firmware doesn't exist, grab it now */
	basename = g_strdup_printf ("purism-%s.bin", id);
	firmware_orig = g_build_filename (LOCALSTATEDIR, "lib", "fwupd",
					  "builder", basename, NULL);
	if (!fu_common_mkdir_parent (firmware_orig, error))
		return FALSE;
	if (!g_file_test (firmware_orig, G_FILE_TEST_EXISTS)) {
		const gchar *argv[] = {
			flashrom_fn,
			"--programmer", "internal:laptop=force_I_want_a_brick",
			"--read", firmware_orig,
			"--verbose", NULL };
		if (!fu_common_spawn_sync ((const gchar * const *) argv,
					   fu_plugin_purism_read_cb, plugin,
					   NULL, error)) {
			g_prefix_error (error, "failed to get original firmware: ");
			return NULL;
		}
		fu_plugin_set_status (plugin, FWUPD_STATUS_IDLE);
	}

	/* create device */
	fu_device_set_id (dev, device_id);
	fu_device_add_flag (dev, FWUPD_DEVICE_FLAG_INTERNAL);
	fu_device_add_flag (dev, FWUPD_DEVICE_FLAG_ALLOW_ONLINE);
	fu_device_add_flag (dev, FWUPD_DEVICE_FLAG_ALLOW_OFFLINE);
	fu_device_add_guid (dev, guid);
	fu_device_set_version (dev, fu_plugin_get_dmi_value (plugin, FU_HWIDS_KEY_BIOS_VERSION));
	fu_device_set_name (dev, fu_plugin_get_dmi_value (plugin, FU_HWIDS_KEY_PRODUCT_NAME));
	fu_device_set_vendor (dev, fu_plugin_get_dmi_value (plugin, FU_HWIDS_KEY_MANUFACTURER));
	return g_steal_pointer (&dev);
}

gboolean
fu_plugin_update_online (FuPlugin *plugin,
			 FuDevice *device,
			 GBytes *blob_fw,
			 FwupdInstallFlags flags,
			 GError **error)
{
	g_autofree gchar *flashrom_fn = NULL;
	g_autofree gchar *firmware_fn = NULL;
	g_autofree gchar *tmpdir = NULL;
	const gchar *argv[] = {
		"/xxx/flashrom",
		"--programmer", "internal:laptop=force_I_want_a_brick",
		"--write", "xxx",
		"--verbose", NULL };

	/* we need flashrom from the host system */
	flashrom_fn = g_find_program_in_path ("flashrom");
	if (flashrom_fn == NULL) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_NOT_SUPPORTED,
				     "flashrom is not installed");
		return FALSE;
	}

	/* write blob to temp location */
	tmpdir = g_dir_make_tmp ("fwupd-XXXXXX", error);
	if (tmpdir == NULL)
		return FALSE;
	firmware_fn = g_build_filename (tmpdir, "purism-firmware.bin", NULL);
	if (!fu_common_set_contents_bytes (firmware_fn, blob_fw, error))
		return FALSE;

	/* FIXME: remove */
	if (g_getenv ("FORCE_I_WANT_A_BRICK") == NULL) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_INTERNAL,
				     "failed as I'm not that brave, yet");
		return FALSE;
	}

	/* use flashrom to write image */
	argv[0] = flashrom_fn;
	argv[4] = firmware_fn;
	if (!fu_common_spawn_sync ((const gchar * const *) argv,
				   fu_plugin_purism_write_cb, plugin,
				   NULL, error)) {
		g_prefix_error (error, "failed to write firmware: ");
		return FALSE;
	}

	/* delete temp location */
	if (!fu_common_rmtree (tmpdir, error))
		return FALSE;

	/* success */
	fu_plugin_set_status (plugin, FWUPD_STATUS_IDLE);
	return TRUE;
}

gboolean
fu_plugin_startup (FuPlugin *plugin, GError **error)
{
	struct {
		const gchar *guid;
		const gchar *id;
	} devices[] = {
		{ "a0ce5085-2dea-5086-ae72-45810a186ad0",	"librem15v3" },
		{ NULL, NULL }
	};

	/* search for devices */
	for (guint i = 0; devices[i].guid != NULL; i++) {
		if (fu_plugin_check_hwid (plugin, devices[i].guid)) {
			g_autoptr(FuDevice) dev = NULL;
			dev = fu_plugin_purism_create_device (plugin,
							      devices[i].guid,
							      devices[i].id,
							      error);
			if (dev == NULL)
				return FALSE;
			fu_plugin_device_add (plugin, dev);
		}
	}
	return TRUE;
}
