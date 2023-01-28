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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>

#include <xf86drm.h>
#include <drm_fourcc.h>

#include "gbm.h"
#include "tegra_udrm_gbm_int.h"

static uint32_t
format_canonicalize(uint32_t gbm_format)
{
   switch (gbm_format) {
   case GBM_BO_FORMAT_XRGB8888:
      return GBM_FORMAT_XRGB8888;
   case GBM_BO_FORMAT_ARGB8888:
      return GBM_FORMAT_ARGB8888;
   default:
      return gbm_format;
   }
}

static int
gbm_tudrm_is_format_supported(struct gbm_device *gbm,
                              uint32_t format,
                              uint32_t usage)
{
    switch (format) {
    case GBM_FORMAT_XRGB8888:
    case GBM_FORMAT_ARGB8888:
    case GBM_FORMAT_XBGR8888:
    case GBM_FORMAT_ABGR8888:
        return 1;
    default:
        return 0;
    }
}

static int
gbm_tudrm_get_format_modifier_plane_count(struct gbm_device *gbm,
                                          uint32_t format,
                                          uint64_t modifier)
{
    return 1;
}

static int
gbm_tudrm_bo_write(struct gbm_bo *_bo, const void *buf, size_t count)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);
    return 0;
}

static int
gbm_tudrm_bo_get_fd(struct gbm_bo *_bo)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);
    return bo->data.dmabuf_fd;
}

static int
gbm_tudrm_bo_get_planes(struct gbm_bo *_bo)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);
    return 1;
}

static union gbm_bo_handle
gbm_tudrm_bo_get_handle_for_plane(struct gbm_bo *_bo, int plane)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);
    return bo->base.v0.handle;
}

static uint32_t
gbm_tudrm_bo_get_stride(struct gbm_bo *_bo, int plane)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);
    return bo->base.v0.stride;
}

static uint32_t
gbm_tudrm_bo_get_offset(struct gbm_bo *_bo, int plane)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);
    return 0;
}

static uint64_t
gbm_tudrm_bo_get_modifier(struct gbm_bo *_bo)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);
    return bo->data.modifier;
}

static struct gbm_bo *
gbm_tudrm_bo_import(struct gbm_device *gbm,
                  uint32_t type, void *buffer, uint32_t usage)
{

    struct gbm_tudrm_device *dri = gbm_tudrm_device(gbm);
    struct gbm_tudrm_bo *bo;

    bo = calloc(1, sizeof *bo);
    if (bo == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    bo->base.gbm = gbm;

    if (type == GBM_BO_IMPORT_FD_MODIFIER) {
        int ret;
        struct gbm_import_fd_modifier_data *fd_data = buffer;

        struct gbm_import_fd_data import_data = {
            .fd = fd_data->fds[0],
            .format = fd_data->format,
            .width = fd_data->width,
            .height = fd_data->height,
            .stride = fd_data->strides[0],
        };

        int dmabuf_fd = fd_data->fds[0];
        uint32_t handle = 0;

        // TODO: more than one plane?
        ret = drmPrimeFDToHandle(dri->base.v0.fd, dmabuf_fd, &handle);
        if (ret < 0) {
            goto fail;
        }

        bo->base.v0.handle.u32 = handle;
        bo->base.v0.width = fd_data->width;
        bo->base.v0.height = fd_data->height;
        bo->base.v0.format = format_canonicalize(fd_data->format);
        bo->base.v0.stride = fd_data->strides[0];
        bo->data.dmabuf_fd = fd_data->fds[0];
        bo->data.modifier = fd_data->modifier;

    } else if (type == GBM_BO_IMPORT_FD) {

        int ret;
        struct gbm_import_fd_data *fd_data = buffer;

        int dmabuf_fd = fd_data->fd;
        uint32_t handle = 0;

        ret = drmPrimeFDToHandle(dri->base.v0.fd, dmabuf_fd, &handle);
        if (ret < 0) {
            goto fail;
        }

        bo->base.v0.handle.u32 = handle;
        bo->base.v0.width = fd_data->width;
        bo->base.v0.height = fd_data->height;
        bo->base.v0.format = format_canonicalize(fd_data->format);
        bo->base.v0.stride = fd_data->stride;
        bo->data.dmabuf_fd = fd_data->fd;

    } else {
        // TODO: maybe GBM_BO_IMPORT_EGL_IMAGE
        // or GBM_BO_IMPORT_WL_BUFFER?
        goto fail;
    }

    return &bo->base;

fail:
    free(bo);
    return NULL;
}

static inline void *
gbm_tudrm_bo_map_dumb(struct gbm_tudrm_device *dri, struct gbm_tudrm_bo *bo)
{
   struct drm_mode_map_dumb map_arg;
   int ret;

   if (bo->data.map != NULL)
      return bo->data.map;

   memset(&map_arg, 0, sizeof(map_arg));
   map_arg.handle = bo->data.handle;

   ret = drmIoctl(dri->base.v0.fd, DRM_IOCTL_MODE_MAP_DUMB, &map_arg);
   if (ret)
      return NULL;

   bo->data.map = mmap(NULL, bo->data.size, PROT_WRITE,
                  MAP_SHARED, dri->base.v0.fd, map_arg.offset);
   if (bo->data.map == MAP_FAILED) {
      bo->data.map = NULL;
      return NULL;
   }

   return bo->data.map;
}

static struct gbm_bo *
gbm_tudrm_bo_create(struct gbm_device *gbm,
                  uint32_t width, uint32_t height,
                  uint32_t format, uint32_t usage,
                  const uint64_t *_modifiers,
                  const unsigned int count)
{
    struct gbm_tudrm_device *dri = gbm_tudrm_device(gbm);
    struct gbm_tudrm_bo *bo;

    bo = calloc(1, sizeof *bo);
    if (bo == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    bo->base.gbm = gbm;
    bo->base.v0.width = width;
    bo->base.v0.height = height;
    bo->base.v0.format = format_canonicalize(format);
    bo->data.modifier = (count && _modifiers) ? _modifiers[0] : 0;

    if (usage & GBM_BO_USE_WRITE) {
        struct drm_mode_create_dumb create_arg;
        int ret;

        memset(&create_arg, 0, sizeof(create_arg));
        create_arg.bpp = 32;
        create_arg.width = width;
        create_arg.height = height;

        ret = drmIoctl(dri->base.v0.fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_arg);
        if (ret) {
            free(bo);
            return NULL;
        }

        bo->base.v0.stride = create_arg.pitch;
        bo->base.v0.format = format;
        bo->base.v0.handle.u32 = create_arg.handle;
        bo->data.handle = create_arg.handle;
        bo->data.size = create_arg.size;

        if (gbm_tudrm_bo_map_dumb(dri, bo) == NULL) {
            struct drm_mode_destroy_dumb destroy_arg;
            memset(&destroy_arg, 0, sizeof destroy_arg);
            destroy_arg.handle = create_arg.handle;
            drmIoctl(dri->base.v0.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_arg);
        }

    } else {

        // TODO: handle these cases:
        /*

        if (usage & GBM_BO_USE_SCANOUT)
            dri_use |= __DRI_IMAGE_USE_SCANOUT;
        if (usage & GBM_BO_USE_CURSOR)
            dri_use |= __DRI_IMAGE_USE_CURSOR;
        if (usage & GBM_BO_USE_LINEAR)
            dri_use |= __DRI_IMAGE_USE_LINEAR;
        if (usage & GBM_BO_USE_PROTECTED)
            dri_use |= __DRI_IMAGE_USE_PROTECTED;
        if (usage & GBM_BO_USE_FRONT_RENDERING)
            dri_use |= __DRI_IMAGE_USE_FRONT_RENDERING;

        see also gbm_dri_bo_create / loader_dri_create_image

        */

        return NULL;
    }

    return &bo->base;
}

static void
gbm_tudrm_bo_destroy(struct gbm_bo *_bo)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);
    free(bo);
}

static void *
gbm_tudrm_bo_map(struct gbm_bo *_bo,
              uint32_t x, uint32_t y,
              uint32_t width, uint32_t height,
              uint32_t flags, uint32_t *stride, void **map_data)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);

    /* If it's a dumb buffer, we already have a mapping */
    if (bo->data.map) {
        *map_data = (char *)bo->data.map + (bo->base.v0.stride * y) + (x * 4);
        *stride = bo->base.v0.stride;
        return *map_data;
    }

    return NULL;
}

static void
gbm_tudrm_bo_unmap(struct gbm_bo *_bo, void *map_data)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);

    /* Check if it's a dumb buffer and check the pointer is in range */
    if (bo->data.map) {
        assert(map_data >= bo->data.map);
        assert(map_data < (bo->data.map + bo->data.size));
        return;
    }
}

static void
gbm_tudrm_surface_destroy(struct gbm_surface *_surf)
{
    struct gbm_tudrm_surface *surf = gbm_tudrm_surface(_surf);
    if (surf->base.v0.modifiers)
        free(surf->base.v0.modifiers);
    free(surf);
}

static struct gbm_surface *
gbm_tudrm_surface_create(struct gbm_device *gbm,
                       uint32_t width, uint32_t height,
                       uint32_t format, uint32_t flags,
                       const uint64_t *modifiers, const unsigned count)
{
    struct gbm_tudrm_device *dri = gbm_tudrm_device(gbm);
    struct gbm_tudrm_surface *surf;

    /* It's acceptable to create an image with INVALID modifier in the list,
        * but it cannot be on the only modifier (since it will certainly fail
        * later). While we could easily catch this after modifier creation, doing
        * the check here is a convenient debug check likely pointing at whatever
        * interface the client is using to build its modifier list.
        */
    if (count == 1 && modifiers[0] == DRM_FORMAT_MOD_INVALID) {
        fprintf(stderr, "Only invalid modifier specified\n");
        errno = EINVAL;
    }

    surf = calloc(1, sizeof *surf);
    if (surf == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    surf->base.gbm = gbm;
    surf->base.v0.width = width;
    surf->base.v0.height = height;
    surf->base.v0.format = format_canonicalize(format);
    surf->base.v0.flags = flags;
    if (!modifiers) {
        assert(!count);
        // if the buffer is being used for scanout, make sure it's linear
        if (flags & GBM_BO_USE_SCANOUT) {
            surf->base.v0.modifiers = calloc(count, sizeof(uint64_t));
            surf->base.v0.modifiers[0] = DRM_FORMAT_MOD_LINEAR;
            surf->base.v0.count = 1;
        }
        return &surf->base;
    }

    surf->base.v0.modifiers = calloc(count, sizeof(*modifiers));
    if (count && !surf->base.v0.modifiers) {
        errno = ENOMEM;
        free(surf);
        return NULL;
    }

    uint64_t *v0_modifiers = surf->base.v0.modifiers;
    for (int i = 0; i < count; i++) {
        // compressed buffers don't render correctly when imported
        if (modifiers[i] & ~DRM_FORMAT_MOD_NVIDIA_BLOCK_LINEAR_2D(0x0, 0x1, 0x3, 0xff, 0xf))
            continue;
        *v0_modifiers++ = modifiers[i];
    }
    surf->base.v0.count = v0_modifiers - surf->base.v0.modifiers;

    return &surf->base;
}

static void
gbm_tudrm_device_destroy(struct gbm_device *gbm)
{
    struct gbm_tudrm_device *tudrm = gbm_tudrm_device(gbm);
    free(tudrm);
}

static struct gbm_device *
gbm_tudrm_device_create(int fd, uint32_t gbm_backend_version)
{
    struct gbm_tudrm_device *tudrm;

    if (gbm_backend_version != GBM_BACKEND_ABI_VERSION) {
        errno = EINVAL;
        return NULL;
    }

    tudrm = calloc(1, sizeof *tudrm);
    if (!tudrm) {
        errno = ENOMEM;
        return NULL;
    }

    tudrm->base.v0.backend_version = GBM_BACKEND_ABI_VERSION;
    tudrm->base.v0.name = "tegra-udrm";
    tudrm->base.v0.fd = fd;
    tudrm->base.v0.destroy = gbm_tudrm_device_destroy;
    tudrm->base.v0.is_format_supported = gbm_tudrm_is_format_supported;
    tudrm->base.v0.get_format_modifier_plane_count = gbm_tudrm_get_format_modifier_plane_count;
    tudrm->base.v0.bo_create = gbm_tudrm_bo_create;
    tudrm->base.v0.bo_destroy = gbm_tudrm_bo_destroy;
    tudrm->base.v0.bo_import = gbm_tudrm_bo_import;
    tudrm->base.v0.bo_map = gbm_tudrm_bo_map;
    tudrm->base.v0.bo_unmap = gbm_tudrm_bo_unmap;
    tudrm->base.v0.bo_write = gbm_tudrm_bo_write;
    tudrm->base.v0.bo_get_fd = gbm_tudrm_bo_get_fd;
    tudrm->base.v0.bo_get_planes = gbm_tudrm_bo_get_planes;
    tudrm->base.v0.bo_get_handle = gbm_tudrm_bo_get_handle_for_plane;
    tudrm->base.v0.bo_get_stride = gbm_tudrm_bo_get_stride;
    tudrm->base.v0.bo_get_offset = gbm_tudrm_bo_get_offset;
    tudrm->base.v0.bo_get_modifier = gbm_tudrm_bo_get_modifier;
    tudrm->base.v0.surface_create = gbm_tudrm_surface_create;
    tudrm->base.v0.surface_destroy = gbm_tudrm_surface_destroy;

    /*

   dri->base.v0.bo_get_plane_fd = gbm_dri_bo_get_plane_fd;
   dri->base.v0.bo_get_stride = gbm_dri_bo_get_stride;
   dri->base.v0.bo_get_offset = gbm_dri_bo_get_offset;
   dri->base.v0.bo_get_modifier = gbm_dri_bo_get_modifier;
   dri->base.v0.bo_destroy = gbm_dri_bo_destroy;
   dri->base.v0.destroy = dri_destroy;
   dri->base.v0.surface_create = gbm_dri_surface_create;
   dri->base.v0.surface_destroy = gbm_dri_surface_destroy;

*/

    return &tudrm->base;
}

struct gbm_backend gbm_backend = {
    .v0.backend_version = GBM_BACKEND_ABI_VERSION,
    .v0.backend_name = "tegra-udrm",
    .v0.create_device = gbm_tudrm_device_create,
};

GBM_EXPORT struct gbm_backend* 
gbmint_get_backend(const struct gbm_core *gbm_core) {
    return &gbm_backend;
}
