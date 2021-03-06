/* $Id$ */
/* Copyright (c) 2013 Pierre Pronchery <khorben@defora.org> */
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



#ifndef CAMERA_OVERLAY_H
# define CAMERA_OVERLAY_H


/* public */
/* types */
typedef struct _CameraOverlay CameraOverlay;


/* functions */
CameraOverlay * cameraoverlay_new(char const * filename, int opacity);
void cameraoverlay_delete(CameraOverlay * overlay);

/* accessors */
int cameraoverlay_get_opacity(CameraOverlay * overlay);
void cameraoverlay_set_opacity(CameraOverlay * overlay, int opacity);

/* useful */
void cameraoverlay_blit(CameraOverlay * overlay, GdkPixbuf * dest);

#endif /* !CAMERA_OVERLAY_H */
