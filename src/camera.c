/* $Id$ */
static char const _copyright[] =
"Copyright (c) 2012-2013 Pierre Pronchery <khorben@defora.org>";
/* This file is part of DeforaOS Desktop Camera */
static char const _license[] =
"This program is free software: you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation, version 3 of the License.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License\n"
"along with this program.  If not, see <http://www.gnu.org/licenses/>.";



#include <sys/ioctl.h>
#include <sys/stat.h>
#ifdef __NetBSD__
# include <sys/videoio.h>
#else
# include <linux/videodev2.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#ifdef DEBUG
# include <stdio.h>
#endif
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <System.h>
#include <Desktop.h>
#include "camera.h"
#include "../config.h"
#define _(string) gettext(string)
#define N_(string) (string)


/* Camera */
/* private */
/* types */
struct _Camera
{
	String * device;

	guint source;
	int fd;
	struct v4l2_capability cap;
	struct v4l2_format format;

	/* IO channel */
	GIOChannel * channel;

	/* input data */
	char * raw_buffer;
	size_t raw_buffer_cnt;

	/* RGB data */
	unsigned char * rgb_buffer;
	size_t rgb_buffer_cnt;

	/* decoding */
	int yuv_amp;

	/* widgets */
	PangoFontDescription * bold;
	GdkGC * gc;
	GtkWidget * window;
#if GTK_CHECK_VERSION(2, 18, 0)
	GtkWidget * infobar;
	GtkWidget * infobar_label;
#endif
	GtkWidget * area;
	GtkAllocation area_allocation;
	GdkPixmap * pixmap;
};


/* prototypes */
static int _camera_error(Camera * camera, char const * message, int ret);

static int _camera_ioctl(Camera * camera, unsigned long request,
		void * data);

/* callbacks */
static gboolean _camera_on_can_read(GIOChannel * channel,
		GIOCondition condition, gpointer data);
static void _camera_on_close(gpointer data);
static gboolean _camera_on_closex(gpointer data);
static gboolean _camera_on_drawing_area_configure(GtkWidget * widget,
		GdkEventConfigure * event, gpointer data);
static gboolean _camera_on_drawing_area_expose(GtkWidget * widget,
		GdkEventExpose * event, gpointer data);
static gboolean _camera_on_open(gpointer data);
static void _camera_on_preferences(gpointer data);
static void _camera_on_properties(gpointer data);
static gboolean _camera_on_refresh(gpointer data);
static void _camera_on_snapshot(gpointer data);

#ifndef EMBEDDED
/* menus */
static void _camera_on_file_close(gpointer data);
static void _camera_on_file_properties(gpointer data);
static void _camera_on_file_snapshot(gpointer data);
static void _camera_on_edit_preferences(gpointer data);
static void _camera_on_help_about(gpointer data);
#endif


/* constants */
#ifndef EMBEDDED
static char const * _authors[] =
{
	"Pierre Pronchery <khorben@defora.org>",
	NULL
};
#endif

#ifdef EMBEDDED
static const DesktopAccel _camera_accel[] =
{
	{ G_CALLBACK(_camera_on_close), GDK_CONTROL_MASK, GDK_KEY_W },
	{ NULL, 0, 0 }
};
#endif

#ifndef EMBEDDED
/* menus */
static const DesktopMenu _camera_menu_file[] =
{
	{ "Take _snapshot", G_CALLBACK(_camera_on_file_snapshot),
		"camera-photo", 0, 0 },
	{ "", NULL, NULL, 0, 0 },
	{ "_Properties", G_CALLBACK(_camera_on_file_properties),
		GTK_STOCK_PROPERTIES, GDK_MOD1_MASK, GDK_KEY_Return },
	{ "", NULL, NULL, 0, 0 },
	{ "_Close", G_CALLBACK(_camera_on_file_close), GTK_STOCK_CLOSE,
		GDK_CONTROL_MASK, GDK_KEY_W },
	{ NULL, NULL, NULL, 0, 0 }
};

static const DesktopMenu _camera_menu_edit[] =
{
	{ "_Preferences", G_CALLBACK(_camera_on_edit_preferences),
		GTK_STOCK_PREFERENCES, GDK_CONTROL_MASK, GDK_KEY_P },
	{ NULL, NULL, NULL, 0, 0 }
};

static const DesktopMenu _camera_menu_help[] =
{
	{ "_About", G_CALLBACK(_camera_on_help_about), GTK_STOCK_ABOUT, 0, 0 },
	{ NULL, NULL, NULL, 0, 0 }
};

static const DesktopMenubar _camera_menubar[] =
{
	{ "_File", _camera_menu_file },
	{ "_Edit", _camera_menu_edit },
	{ "_Help", _camera_menu_help },
	{ NULL, NULL }
};
#endif


/* variables */
static DesktopToolbar _camera_toolbar[] =
{
	{ "Snapshot", G_CALLBACK(_camera_on_snapshot), "camera-photo", 0, 0,
		NULL },
#ifdef EMBEDDED
	{ "", NULL, NULL, 0, 0, NULL },
	{ "Properties", G_CALLBACK(_camera_on_properties),
		GTK_STOCK_PROPERTIES, GDK_MOD1_MASK, GDK_KEY_Return, NULL },
	{ "", NULL, NULL, 0, 0, NULL },
	{ "Preferences", G_CALLBACK(_camera_on_preferences),
		GTK_STOCK_PREFERENCES, GDK_CONTROL_MASK, GDK_KEY_P, NULL },
#endif
	{ NULL, NULL, NULL, 0, 0, NULL }
};


/* public */
/* functions */
/* camera_new */
Camera * camera_new(char const * device)
{
	Camera * camera;
	GtkAccelGroup * group;
	GtkWidget * vbox;
	GtkWidget * widget;

	if((camera = object_new(sizeof(*camera))) == NULL)
		return NULL;
	if(device == NULL)
		device = "/dev/video0";
	camera->device = string_new(device);
	camera->source = 0;
	camera->fd = -1;
	memset(&camera->cap, 0, sizeof(camera->cap));
	camera->channel = NULL;
	camera->raw_buffer = NULL;
	camera->raw_buffer_cnt = 0;
	camera->rgb_buffer = NULL;
	camera->rgb_buffer_cnt = 0;
	camera->yuv_amp = 255;
	camera->bold = NULL;
	camera->gc = NULL;
	camera->window = NULL;
	/* check for errors */
	if(camera->device == NULL)
	{
		camera_delete(camera);
		return NULL;
	}
	/* create the window */
	camera->bold = pango_font_description_new();
	pango_font_description_set_weight(camera->bold, PANGO_WEIGHT_BOLD);
	group = gtk_accel_group_new();
	camera->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(camera->window);
	camera->gc = gdk_gc_new(camera->window->window); /* XXX */
	gtk_window_add_accel_group(GTK_WINDOW(camera->window), group);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_window_set_icon_name(GTK_WINDOW(camera->window), "camera-photo");
#endif
	gtk_window_set_title(GTK_WINDOW(camera->window), "Camera");
	g_signal_connect_swapped(camera->window, "delete-event", G_CALLBACK(
				_camera_on_closex), camera);
	vbox = gtk_vbox_new(FALSE, 0);
#ifdef EMBEDDED
	desktop_accel_create(_camera_accel, camera, group);
#endif
#ifndef EMBEDDED
	/* menubar */
	widget = desktop_menubar_create(_camera_menubar, camera, group);
	gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, TRUE, 0);
#endif
	/* toolbar */
	widget = desktop_toolbar_create(_camera_toolbar, camera, group);
	gtk_widget_set_sensitive(GTK_WIDGET(_camera_toolbar[0].widget), FALSE);
#ifdef EMBEDDED
	gtk_widget_set_sensitive(GTK_WIDGET(_camera_toolbar[2].widget), FALSE);
#endif
	gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, TRUE, 0);
#if GTK_CHECK_VERSION(2, 18, 0)
	/* infobar */
	camera->infobar = gtk_info_bar_new_with_buttons(GTK_STOCK_CLOSE,
			GTK_RESPONSE_CLOSE, NULL);
	gtk_info_bar_set_message_type(GTK_INFO_BAR(camera->infobar),
			GTK_MESSAGE_ERROR);
	g_signal_connect(camera->infobar, "close", G_CALLBACK(gtk_widget_hide),
			NULL);
	g_signal_connect(camera->infobar, "response", G_CALLBACK(
				gtk_widget_hide), NULL);
	widget = gtk_info_bar_get_content_area(GTK_INFO_BAR(camera->infobar));
	camera->infobar_label = gtk_label_new(NULL);
	gtk_widget_show(camera->infobar_label);
	gtk_box_pack_start(GTK_BOX(widget), camera->infobar_label, TRUE, TRUE,
			0);
	gtk_widget_set_no_show_all(camera->infobar, TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), camera->infobar, FALSE, TRUE, 0);
#endif
	camera->area = gtk_drawing_area_new();
	camera->pixmap = NULL;
	g_signal_connect(camera->area, "configure-event", G_CALLBACK(
				_camera_on_drawing_area_configure), camera);
	g_signal_connect(camera->area, "expose-event", G_CALLBACK(
				_camera_on_drawing_area_expose), camera);
	gtk_box_pack_start(GTK_BOX(vbox), camera->area, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(camera->window), vbox);
	gtk_widget_show_all(camera->window);
	camera->source = g_idle_add(_camera_on_open, camera);
	return camera;
}


/* camera_delete */
void camera_delete(Camera * camera)
{
	if(camera->source != 0)
		g_source_remove(camera->source);
	if(camera->channel != NULL)
	{
		/* XXX we ignore errors at this point */
		g_io_channel_shutdown(camera->channel, TRUE, NULL);
#if 0
		/* FIXME seems to cause a crash */
		g_object_unref(camera->channel);
#endif
	}
	if(camera->pixmap != NULL)
		g_object_unref(camera->pixmap);
	if(camera->gc != NULL)
		g_object_unref(camera->gc);
	if(camera->window != NULL)
		gtk_widget_destroy(camera->window);
	if(camera->bold != NULL)
		pango_font_description_free(camera->bold);
	if(camera->fd >= 0)
		close(camera->fd);
	if((char *)camera->rgb_buffer != camera->raw_buffer)
		free(camera->rgb_buffer);
	free(camera->raw_buffer);
	string_delete(camera->device);
	object_delete(camera);
}


/* useful */
/* camera_snapshot */
static int _snapshot_dcim(Camera * camera, char const * homedir,
		char const * dcim);
static char * _snapshot_path(Camera * camera, char const * homedir,
		char const * dcim, char const * extension);
static int _snapshot_save(Camera * camera, char const * path);

int camera_snapshot(Camera * camera)
{
	int ret;
	char const * homedir;
	char const dcim[] = "DCIM";
	char const ext[] = ".png";
	char * path;

	if(camera->rgb_buffer == NULL)
		/* ignore the action */
		return 0;
	if((homedir = getenv("HOME")) == NULL)
		homedir = g_get_home_dir();
	if(_snapshot_dcim(camera, homedir, dcim) != 0)
		return -1;
	if((path = _snapshot_path(camera, homedir, dcim, ext)) == NULL)
		return -1;
	ret = _snapshot_save(camera, path);
	free(path);
	return ret;
}

static int _snapshot_dcim(Camera * camera, char const * homedir,
		char const * dcim)
{
	char * path;

	if((path = g_build_filename(homedir, dcim, NULL)) == NULL)
		return -_camera_error(camera, _("Could not save picture"), 1);
	if(mkdir(path, 0777) != 0 && errno != EEXIST)
	{
		error_set_code(1, "%s: %s: %s", _("Could not save picture"),
				path, strerror(errno));
		free(path);
		return -_camera_error(camera, error_get(), 1);
	}
	free(path);
	return 0;
}

static char * _snapshot_path(Camera * camera, char const * homedir,
		char const * dcim, char const * extension)
{
	struct timeval tv;
	struct tm tm;
	unsigned int i;
	char * filename;
	char * path;

	if(gettimeofday(&tv, NULL) != 0 || gmtime_r(&tv.tv_sec, &tm) == NULL)
	{
		error_set_code(1, "%s: %s", _("Could not save picture"),
				strerror(errno));
		_camera_error(camera, error_get(), 1);
		return NULL;
	}
	for(i = 0; i < 64; i++)
	{
		if((filename = g_strdup_printf("%u%02u%02u-%02u%02u%02u-%03u%s",
						tm.tm_year + 1900,
						tm.tm_mon + 1, tm.tm_mday,
						tm.tm_hour, tm.tm_min,
						tm.tm_sec, i + 1, extension))
				== NULL)
			/* XXX report error */
			return NULL;
		path = g_build_filename(homedir, dcim, filename, NULL);
		g_free(filename);
		if(path == NULL)
		{
			_camera_error(camera, _("Could not save picture"), 1);
			return NULL;
		}
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() %s\n", __func__, path);
#endif
		if(access(path, R_OK) != 0 && errno == ENOENT)
			return path;
		g_free(path);
	}
	return NULL;
}

static int _snapshot_save(Camera * camera, char const * path)
{
	struct v4l2_pix_format * pix = &camera->format.fmt.pix;
	GdkPixbuf * pixbuf;
	gboolean res;
	GError * error = NULL;

	if((pixbuf = gdk_pixbuf_new_from_data(camera->rgb_buffer,
					GDK_COLORSPACE_RGB, FALSE, 8,
					pix->width, pix->height, pix->width * 3,
					NULL, NULL)) == NULL)
		return -_camera_error(camera, _("Could not save picture"), 1);
	res = gdk_pixbuf_save(pixbuf, path, "png", &error, NULL);
	g_object_unref(pixbuf);
	if(res != TRUE)
	{
		error_set_code(1, "%s: %s", _("Could not save picture"),
				error->message);
		g_error_free(error);
		return -_camera_error(camera, error_get(), 1);
	}
	return 0;
}


/* private */
/* functions */
/* camera_error */
static int _error_text(char const * message, int ret);

static int _camera_error(Camera * camera, char const * message, int ret)
{
#if !GTK_CHECK_VERSION(2, 18, 0)
	GtkWidget * dialog;
#endif

	if(camera == NULL)
		return _error_text(message, ret);
#if GTK_CHECK_VERSION(2, 18, 0)
	gtk_label_set_text(GTK_LABEL(camera->infobar_label), message);
	gtk_widget_show(camera->infobar);
#else
	dialog = gtk_message_dialog_new(GTK_WINDOW(camera->window),
			GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
# if GTK_CHECK_VERSION(2, 6, 0)
			"%s", "Error");
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
# endif
			"%s", message);
	gtk_window_set_title(GTK_WINDOW(dialog), "Error");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
#endif
	return ret;
}

static int _error_text(char const * message, int ret)
{
	fprintf(stderr, "%s: %s\n", PACKAGE, message);
	return ret;
}


/* camera_ioctl */
static int _camera_ioctl(Camera * camera, unsigned long request,
		void * data)
{
	int ret;

	for(;;)
		if((ret = ioctl(camera->fd, request, data)) != -1
				|| errno != EINTR)
			break;
	return ret;
}


/* callbacks */
/* camera_on_can_read */
static gboolean _camera_on_can_read(GIOChannel * channel,
		GIOCondition condition, gpointer data)
{
	Camera * camera = data;
	ssize_t s;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(channel != camera->channel || condition != G_IO_IN)
		return FALSE;
	if((s = read(camera->fd, camera->raw_buffer, camera->raw_buffer_cnt))
			<= 0)
	{
		/* this error can be ignored */
		if(errno == EAGAIN)
			return TRUE;
		close(camera->fd);
		camera->fd = -1;
		_camera_error(camera, strerror(errno), 1);
		gtk_widget_set_sensitive(GTK_WIDGET(_camera_toolbar[0].widget),
				FALSE);
#ifdef EMBEDDED
		gtk_widget_set_sensitive(GTK_WIDGET(_camera_toolbar[2].widget),
				FALSE);
#endif
		camera->source = 0;
		return FALSE;
	}
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %lu %ld\n", __func__,
			camera->raw_buffer_cnt, s);
#endif
	camera->source = g_idle_add(_camera_on_refresh, camera);
	return FALSE;
}


/* camera_on_close */
static void _camera_on_close(gpointer data)
{
	Camera * camera = data;

	gtk_widget_hide(camera->window);
	if(camera->source != 0)
		g_source_remove(camera->source);
	camera->source = 0;
	gtk_main_quit();
}


/* camera_on_closex */
static gboolean _camera_on_closex(gpointer data)
{
	Camera * camera = data;

	_camera_on_close(camera);
	return TRUE;
}


/* camera_on_drawing_area_configure */
static gboolean _camera_on_drawing_area_configure(GtkWidget * widget,
		GdkEventConfigure * event, gpointer data)
{
	/* XXX this code is inspired from GQcam */
	Camera * camera = data;
	GtkAllocation * allocation = &camera->area_allocation;

	if(camera->pixmap != NULL)
		g_object_unref(camera->pixmap);
	/* FIXME requires Gtk+ 2.18 */
	gtk_widget_get_allocation(widget, allocation);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %dx%d\n", __func__, allocation->width,
			allocation->height);
#endif
	camera->pixmap = gdk_pixmap_new(widget->window, allocation->width,
			allocation->height, -1);
	/* FIXME is it not better to scale the previous pixmap for now? */
	gdk_draw_rectangle(camera->pixmap, camera->gc, TRUE, 0, 0,
			allocation->width, allocation->height);
	return TRUE;
}


/* camera_on_drawing_area_expose */
static gboolean _camera_on_drawing_area_expose(GtkWidget * widget,
		GdkEventExpose * event, gpointer data)
{
	/* XXX this code is inspired from GQcam */
	Camera * camera = data;

	gdk_draw_pixmap(widget->window, camera->gc, camera->pixmap,
			event->area.x, event->area.y,
			event->area.x, event->area.y,
			event->area.width, event->area.height);
	return FALSE;
}


#ifndef EMBEDDED
/* menus */
/* camera_on_file_close */
static void _camera_on_file_close(gpointer data)
{
	Camera * camera = data;

	_camera_on_closex(camera);
}


/* camera_on_file_properties */
static void _camera_on_file_properties(gpointer data)
{
	Camera * camera = data;

	_camera_on_properties(camera);
}


/* camera_on_file_snapshot */
static void _camera_on_file_snapshot(gpointer data)
{
	Camera * camera = data;

	_camera_on_snapshot(camera);
}


/* camera_on_edit_preferences */
static void _camera_on_edit_preferences(gpointer data)
{
	Camera * camera = data;

	_camera_on_preferences(camera);
}


/* camera_on_help_about */
static void _camera_on_help_about(gpointer data)
{
	Camera * camera = data;
	GtkWidget * widget;

	widget = desktop_about_dialog_new();
	gtk_window_set_transient_for(GTK_WINDOW(widget), GTK_WINDOW(
				camera->window));
	desktop_about_dialog_set_authors(widget, _authors);
	desktop_about_dialog_set_comments(widget,
			"Simple camera application for the DeforaOS desktop");
	desktop_about_dialog_set_copyright(widget, _copyright);
	desktop_about_dialog_set_license(widget, _license);
	desktop_about_dialog_set_logo_icon_name(widget, "camera-photo");
	desktop_about_dialog_set_name(widget, PACKAGE);
	desktop_about_dialog_set_version(widget, VERSION);
	desktop_about_dialog_set_website(widget, "http://www.defora.org/");
	gtk_dialog_run(GTK_DIALOG(widget));
	gtk_widget_destroy(widget);
}
#endif


/* camera_on_open */
static int _open_setup(Camera * camera);

static gboolean _camera_on_open(gpointer data)
{
	Camera * camera = data;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, camera->device);
#endif
	camera->source = 0;
	if((camera->fd = open(camera->device, O_RDWR)) < 0)
	{
		error_set_code(1, "%s: %s (%s)", camera->device,
				"Could not open the video capture device",
				strerror(errno));
		_camera_error(camera, error_get(), 1);
		return FALSE;
	}
	if(_open_setup(camera) != 0)
	{
		_camera_error(camera, error_get(), 1);
		close(camera->fd);
		camera->fd = -1;
		return FALSE;
	}
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %dx%d\n", __func__,
			camera->format.fmt.pix.width,
			camera->format.fmt.pix.height);
#endif
	gtk_widget_set_sensitive(GTK_WIDGET(_camera_toolbar[0].widget), TRUE);
#ifdef EMBEDDED
	gtk_widget_set_sensitive(GTK_WIDGET(_camera_toolbar[2].widget), TRUE);
#endif
	gtk_widget_set_size_request(camera->area, camera->format.fmt.pix.width,
			camera->format.fmt.pix.height);
	return FALSE;
}

static int _open_setup(Camera * camera)
{
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	size_t cnt;
	char * p;
	GError * error = NULL;

	/* check for errors */
	if(_camera_ioctl(camera, VIDIOC_QUERYCAP, &camera->cap) == -1)
		return -error_set_code(1, "%s: %s (%s)", camera->device,
				"Could not obtain the capabilities",
				strerror(errno));
	if((camera->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0
			/* FIXME also implement mmap() and streaming */
			|| (camera->cap.capabilities & V4L2_CAP_READWRITE) == 0)
		return -error_set_code(1, "%s: %s", camera->device,
				"Unsupported capabilities");
	/* reset cropping */
	memset(&cropcap, 0, sizeof(cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(_camera_ioctl(camera, VIDIOC_CROPCAP, &cropcap) == 0)
	{
		/* reset to default */
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect;
		if(_camera_ioctl(camera, VIDIOC_S_CROP, &crop) == -1
				&& errno == EINVAL)
			/* XXX ignore this error for now */
			error_set_code(1, "%s: %s", camera->device,
					"Cropping not supported");
	}
	/* obtain the current format */
	if(_camera_ioctl(camera, VIDIOC_G_FMT, &camera->format) == -1)
		return -error_set_code(1, "%s: %s", camera->device,
				"Could not obtain the video capture format");
	/* check the current format */
	if(camera->format.type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -error_set_code(1, "%s: %s", camera->device,
				"Unsupported video capture type");
	/* FIXME also try to obtain a RGB24 format if possible */
	/* allocate the raw buffer */
	cnt = camera->format.fmt.pix.sizeimage;
	if((p = realloc(camera->raw_buffer, cnt)) == NULL)
		return -error_set_code(1, "%s: %s", camera->device,
				strerror(errno));
	camera->raw_buffer = p;
	camera->raw_buffer_cnt = cnt;
	/* allocate the rgb buffer */
	cnt = camera->format.fmt.pix.width * camera->format.fmt.pix.height * 3;
	if((p = realloc(camera->rgb_buffer, cnt)) == NULL)
		return -error_set_code(1, "%s: %s", camera->device,
				strerror(errno));
	camera->rgb_buffer = (unsigned char *)p;
	camera->rgb_buffer_cnt = cnt;
	/* setup a IO channel */
	camera->channel = g_io_channel_unix_new(camera->fd);
	if(g_io_channel_set_encoding(camera->channel, NULL, &error)
			!= G_IO_STATUS_NORMAL)
	{
		error_set_code(1, "%s", error->message);
		g_error_free(error);
		return -1;
	}
	g_io_channel_set_buffered(camera->channel, FALSE);
	camera->source = g_io_add_watch(camera->channel, G_IO_IN,
			_camera_on_can_read, camera);
	return 0;
}


/* camera_on_preferences */
static void _camera_on_preferences(gpointer data)
{
	/* FIXME implement */
}


/* camera_on_properties */
static GtkWidget * _properties_label(Camera * camera, GtkSizeGroup * group,
		char const * label, char const * value);

static void _camera_on_properties(gpointer data)
{
	Camera * camera = data;
	GtkWidget * dialog;
	GtkSizeGroup * group;
	GtkWidget * vbox;
	GtkWidget * hbox;
	char buf[64];

	if(camera->rgb_buffer == NULL)
		/* ignore the action */
		return;
	dialog = gtk_dialog_new_with_buttons("Properties",
			GTK_WINDOW(camera->window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 200);
	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
#if GTK_CHECK_VERSION(2, 14, 0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	vbox = dialog->vbox;
#endif
	/* driver */
	snprintf(buf, sizeof(buf), "%16s", (char *)camera->cap.driver);
	hbox = _properties_label(camera, group, "Driver: ", buf);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	/* card */
	snprintf(buf, sizeof(buf), "%32s", (char *)camera->cap.card);
	hbox = _properties_label(camera, group, "Card: ", buf);
	/* bus info */
	snprintf(buf, sizeof(buf), "%32s", (char *)camera->cap.bus_info);
	hbox = _properties_label(camera, group, "Bus info: ", buf);
	/* version */
	snprintf(buf, sizeof(buf), "0x%x", camera->cap.version);
	hbox = _properties_label(camera, group, "Version: ", buf);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	gtk_widget_show_all(vbox);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

static GtkWidget * _properties_label(Camera * camera, GtkSizeGroup * group,
		char const * label, char const * value)
{
	GtkWidget * hbox;
	GtkWidget * widget;

	hbox = gtk_hbox_new(FALSE, 4);
	widget = gtk_label_new(label);
	gtk_widget_modify_font(widget, camera->bold);
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
	gtk_size_group_add_widget(group, widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	widget = gtk_label_new((value != NULL) ? value : "");
	gtk_label_set_ellipsize(GTK_LABEL(widget), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
	return hbox;
}


/* camera_on_refresh */
static void _refresh_convert(Camera * camera);
static void _refresh_convert_yuv(int amp, uint8_t y, uint8_t u, uint8_t v,
		uint8_t * r, uint8_t * g, uint8_t * b);

static gboolean _camera_on_refresh(gpointer data)
{
	Camera * camera = data;
	GtkAllocation * allocation = &camera->area_allocation;
	int width = camera->format.fmt.pix.width;
	int height = camera->format.fmt.pix.height;
	GdkPixbuf * pixbuf;
	GdkPixbuf * pixbuf2;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() 0x%x\n", __func__,
			camera->format.fmt.pix.pixelformat);
#endif
	_refresh_convert(camera);
	if(width == allocation->width && height == allocation->height)
		/* render directly */
		gdk_draw_rgb_image(camera->pixmap, camera->gc, 0, 0,
				width, height, GDK_RGB_DITHER_NORMAL,
				camera->rgb_buffer, width * 3);
	else
	{
		/* render after scaling */
		pixbuf = gdk_pixbuf_new_from_data(camera->rgb_buffer,
				GDK_COLORSPACE_RGB, FALSE, 8, width, height,
				width * 3, NULL, NULL);
		pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, allocation->width,
				allocation->height, GDK_INTERP_BILINEAR);
		gdk_pixbuf_render_to_drawable(pixbuf2, camera->pixmap,
				camera->gc, 0, 0, 0, 0, -1, -1,
				GDK_RGB_DITHER_NORMAL, 0, 0);
		g_object_unref(pixbuf2);
		g_object_unref(pixbuf);
	}
	/* force a refresh */
	gtk_widget_queue_draw_area(camera->area, 0, 0,
			camera->area_allocation.width,
			camera->area_allocation.height);
	camera->source = g_io_add_watch(camera->channel, G_IO_IN,
			_camera_on_can_read, camera);
	return FALSE;
}

static void _refresh_convert(Camera * camera)
{
	size_t i;
	size_t j;

	switch(camera->format.fmt.pix.pixelformat)
	{
		case V4L2_PIX_FMT_YUYV:
			for(i = 0, j = 0; i + 3 < camera->raw_buffer_cnt;
					i += 4, j += 6)
			{
				/* pixel 0 */
				_refresh_convert_yuv(camera->yuv_amp,
						camera->raw_buffer[i],
						camera->raw_buffer[i + 1],
						camera->raw_buffer[i + 3],
						&camera->rgb_buffer[j + 2],
						&camera->rgb_buffer[j + 1],
						&camera->rgb_buffer[j]);
				/* pixel 1 */
				_refresh_convert_yuv(camera->yuv_amp,
						camera->raw_buffer[i + 2],
						camera->raw_buffer[i + 1],
						camera->raw_buffer[i + 3],
						&camera->rgb_buffer[j + 5],
						&camera->rgb_buffer[j + 4],
						&camera->rgb_buffer[j + 3]);
			}
			break;
		default:
#ifdef DEBUG
			fprintf(stderr, "DEBUG: %s() Unsupported format\n",
					__func__);
#endif
			break;
	}
}

static void _refresh_convert_yuv(int amp, uint8_t y, uint8_t u, uint8_t v,
		uint8_t * r, uint8_t * g, uint8_t * b)
{
	double dr;
	double dg;
	double db;

	dr = amp * (0.004565 * y + 0.007935 * u - 1.088);
	dg = amp * (0.004565 * y - 0.001542 * u - 0.003183 * v + 0.531);
	db = amp * (0.004565 * y + 0.000001 * u + 0.006250 * v - 0.872);
	*r = (dr < 0) ? 0 : ((dr > 255) ? 255 : dr);
	*g = (dg < 0) ? 0 : ((dg > 255) ? 255 : dg);
	*b = (db < 0) ? 0 : ((db > 255) ? 255 : db);
}


/* camera_on_snapshot */
static void _camera_on_snapshot(gpointer data)
{
	Camera * camera = data;

	camera_snapshot(camera);
}
