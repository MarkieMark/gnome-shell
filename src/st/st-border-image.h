/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __ST_BORDER_IMAGE_H__
#define __ST_BORDER_IMAGE_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* A StBorderImage encapsulates an image with specified unscaled borders on each edge.
 */
typedef struct _StBorderImage      StBorderImage;
typedef struct _StBorderImageClass StBorderImageClass;

#define ST_TYPE_BORDER_IMAGE             (st_border_image_get_type ())
#define ST_BORDER_IMAGE(object)          (G_TYPE_CHECK_INSTANCE_CAST ((object), ST_TYPE_BORDER_IMAGE, StBorderImage))
#define ST_BORDER_IMAGE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), ST_TYPE_BORDER_IMAGE, StBorderImageClass))
#define ST_IS_BORDER_IMAGE(object)       (G_TYPE_CHECK_INSTANCE_TYPE ((object), ST_TYPE_BORDER_IMAGE))
#define ST_IS_BORDER_IMAGE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), ST_TYPE_BORDER_IMAGE))
#define ST_BORDER_IMAGE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), ST_TYPE_BORDER_IMAGE, StBorderImageClass))

GType             st_border_image_get_type          (void) G_GNUC_CONST;

StBorderImage *st_border_image_new (const char *filename,
                                    int         border_top,
                                    int         border_right,
                                    int         border_bottom,
                                    int         border_left);

const char *st_border_image_get_filename (StBorderImage *image);
void        st_border_image_get_borders  (StBorderImage *image,
                                          int           *border_top,
                                          int           *border_right,
                                          int           *border_bottom,
                                          int           *border_left);

G_END_DECLS

#endif /* __ST_BORDER_IMAGE_H__ */
