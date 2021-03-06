/* $Id$ */
/* Copyright (c) 2013-2014 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Desktop Camera */
/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */



#ifndef CAMERA_WINDOW_H
# define CAMERA_WINDOW_H

# include "overlay.h"


/* public */
/* types */
typedef struct _CameraWindow CameraWindow;


/* functions */
CameraWindow * camerawindow_new(char const * device);
void camerawindow_delete(CameraWindow * camera);

/* accessors */
void camerawindow_set_aspect_ratio(CameraWindow * camera, gboolean ratio);

void camerawindow_set_fullscreen(CameraWindow * camera, int fullscreen);

void camerawindow_set_hflip(CameraWindow * camera, gboolean flip);
void camerawindow_set_vflip(CameraWindow * camera, gboolean flip);

/* useful */
CameraOverlay * camerawindow_add_overlay(CameraWindow * camera,
		char const * filename, int opacity);

int camerawindow_load(CameraWindow * window);
int camerawindow_save(CameraWindow * window);

#endif /* !CAMERA_WINDOW_H */
