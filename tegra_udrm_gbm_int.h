/*
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _GBM_TUDRM_INTERNAL_H_
#define _GBM_TUDRM_INTERNAL_H_

#include "gbmint.h"
#include <stddef.h>
#include <nvbufsurface.h>

#define ALIGN(val, align) (((val) + (align) - 1) & ~((align) - 1))

#define PAGE_ALIGN(addr)  ALIGN(addr, 4096)

#define BACK_BUFFERS_MAX 10

struct gbm_tudrm_device {
   struct gbm_device base;
};

struct gbm_tudrm_bo_data {
    int dmabuf_fd;
    uint64_t modifier;
    /* Used for cursors and the swrast front BO */
    uint32_t handle, size;
    void *map;
    /* for created buffers */
    NvBufSurface *surface;
};

struct gbm_tudrm_bo {
    struct gbm_bo base;
    struct gbm_tudrm_bo_data data;
};

struct gbm_tudrm_surface {
   void *reserved_for_egl_gbm;
   struct gbm_surface base;
};

static inline struct gbm_tudrm_device *
gbm_tudrm_device(struct gbm_device *gbm)
{
   return (struct gbm_tudrm_device *) gbm;
}

static inline struct gbm_tudrm_bo *
gbm_tudrm_bo(struct gbm_bo *bo)
{
   return (struct gbm_tudrm_bo *) bo;
}

static inline struct gbm_tudrm_surface *
gbm_tudrm_surface(struct gbm_surface *surface)
{
   return (struct gbm_tudrm_surface *)((char *)surface - offsetof(struct gbm_tudrm_surface, base));
}

#endif
