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

#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>

#include <xf86drm.h>
#include <drm_fourcc.h>

#include "gbm.h"
#include "tegra_udrm_gbm_int.h"

static void *libnvgbm;

#define NVGBM_FUNC(F) typedef __typeof__(F) F##_t; F##_t *F = dlsym(libnvgbm, #F);

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
    NVGBM_FUNC(gbm_device_is_format_supported)
    struct gbm_tudrm_device *dri = gbm_tudrm_device(gbm);
    return gbm_device_is_format_supported(dri->nvgbm_device, format, usage);
}

static int
gbm_tudrm_get_format_modifier_plane_count(struct gbm_device *gbm,
                                          uint32_t format,
                                          uint64_t modifier)
{
    NVGBM_FUNC(gbm_device_get_format_modifier_plane_count)
    struct gbm_tudrm_device *dri = gbm_tudrm_device(gbm);
    return gbm_device_get_format_modifier_plane_count(dri->nvgbm_device, format, modifier);
}

static int
gbm_tudrm_bo_write(struct gbm_bo *_bo, const void *buf, size_t count)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);
    if (bo->nvgbm_bo) {
        NVGBM_FUNC(gbm_bo_write)
        return gbm_bo_write(bo->nvgbm_bo, buf, count);
    } else {
        return 0;
    }
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
    if (bo->nvgbm_bo) {
        NVGBM_FUNC(gbm_bo_get_plane_count)
        return gbm_bo_get_plane_count(bo->nvgbm_bo);
    } else {
        return 1;
    }
}

static union gbm_bo_handle
gbm_tudrm_bo_get_handle_for_plane(struct gbm_bo *_bo, int plane)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);
    if (bo->nvgbm_bo) {
        NVGBM_FUNC(gbm_bo_get_handle_for_plane)
        return gbm_bo_get_handle_for_plane(bo->nvgbm_bo, plane);
    }
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
    if (bo->nvgbm_bo) {
        NVGBM_FUNC(gbm_bo_get_offset)
        return gbm_bo_get_offset(bo->nvgbm_bo, plane);
    }
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
    struct gbm_bo *nvgbm_bo;

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
            free(bo);
            return NULL;
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
            free(bo);
            return NULL;
        }

        bo->base.v0.handle.u32 = handle;
        bo->base.v0.width = fd_data->width;
        bo->base.v0.height = fd_data->height;
        bo->base.v0.format = format_canonicalize(fd_data->format);
        bo->base.v0.stride = fd_data->stride;
        bo->data.dmabuf_fd = fd_data->fd;

    } else {
        NVGBM_FUNC(gbm_bo_import)
        NVGBM_FUNC(gbm_bo_get_width)
        NVGBM_FUNC(gbm_bo_get_height)
        NVGBM_FUNC(gbm_bo_get_format)
        NVGBM_FUNC(gbm_bo_get_handle)
        NVGBM_FUNC(gbm_bo_get_stride)

        nvgbm_bo = gbm_bo_import(dri->nvgbm_device, type, buffer, usage);

        if (!nvgbm_bo) {
            free(bo);
            return NULL;
        }

        bo->nvgbm_bo = nvgbm_bo;

        bo->base.v0.width = gbm_bo_get_width(nvgbm_bo);
        bo->base.v0.height = gbm_bo_get_height(nvgbm_bo);
        bo->base.v0.format = gbm_bo_get_format(nvgbm_bo);
        bo->base.v0.handle = gbm_bo_get_handle(nvgbm_bo);
        bo->base.v0.stride = gbm_bo_get_stride(nvgbm_bo);
    }

    return &bo->base;
}

static struct gbm_bo *
gbm_tudrm_bo_create(struct gbm_device *gbm,
                  uint32_t width, uint32_t height,
                  uint32_t format, uint32_t usage,
                  const uint64_t *_modifiers,
                  const unsigned int count)
{
    NVGBM_FUNC(gbm_bo_destroy)
    NVGBM_FUNC(gbm_bo_get_stride)
    NVGBM_FUNC(gbm_bo_get_handle)
    NVGBM_FUNC(gbm_bo_get_fd)

    struct gbm_tudrm_device *dri = gbm_tudrm_device(gbm);
    struct gbm_tudrm_bo *bo;
    struct gbm_bo *nvgbm_bo;

    if (_modifiers && count) {
        NVGBM_FUNC(gbm_bo_create_with_modifiers)

        uint64_t *modifiers = calloc(count, sizeof(uint64_t));
        if (!modifiers) {
            return NULL;
        }

        for (int i = 0; i < count; i++) {
            modifiers[i] = _modifiers[i] & ~fourcc_mod_code(NVIDIA, 0);
        }

        nvgbm_bo = gbm_bo_create_with_modifiers(dri->nvgbm_device, width, height, format, modifiers, count);

        free(modifiers);
    } else {
        NVGBM_FUNC(gbm_bo_create)

        nvgbm_bo = gbm_bo_create(dri->nvgbm_device, width, height, format, usage);
    }

    if (!nvgbm_bo) {
        return NULL;
    }

    bo = calloc(1, sizeof *bo);
    if (bo == NULL) {
        gbm_bo_destroy(nvgbm_bo);
        errno = ENOMEM;
        return NULL;
    }

    bo->nvgbm_bo = nvgbm_bo;

    bo->base.gbm = gbm;
    bo->base.v0.width = width;
    bo->base.v0.height = height;
    bo->base.v0.format = format_canonicalize(format);
    bo->base.v0.handle = gbm_bo_get_handle(nvgbm_bo);
    bo->base.v0.stride = gbm_bo_get_stride(nvgbm_bo);
    bo->data.dmabuf_fd = gbm_bo_get_fd(nvgbm_bo);
    bo->data.modifier = (count && _modifiers) ? _modifiers[0] : 0;

    return &bo->base;
}

static void
gbm_tudrm_bo_destroy(struct gbm_bo *_bo)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);
    if (bo->nvgbm_bo) {
        NVGBM_FUNC(gbm_bo_destroy)
        gbm_bo_destroy(bo->nvgbm_bo);
    }
    free(bo);
}

static void *
gbm_tudrm_bo_map(struct gbm_bo *_bo,
              uint32_t x, uint32_t y,
              uint32_t width, uint32_t height,
              uint32_t flags, uint32_t *stride, void **map_data)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);
    if (bo->nvgbm_bo) {
        NVGBM_FUNC(gbm_bo_map)
        return gbm_bo_map(bo->nvgbm_bo, x, y, width, height, flags, stride, map_data);
    } else {
        return NULL;
    }
}

static void
gbm_tudrm_bo_unmap(struct gbm_bo *_bo, void *map_data)
{
    struct gbm_tudrm_bo *bo = gbm_tudrm_bo(_bo);
    if (bo->nvgbm_bo) {
        NVGBM_FUNC(gbm_bo_unmap)
        gbm_bo_unmap(bo->nvgbm_bo, map_data);
    }
}

static void
gbm_tudrm_surface_destroy(struct gbm_surface *_surf)
{
    // NVGBM_FUNC(gbm_surface_destroy)
    struct gbm_tudrm_surface *surf = gbm_tudrm_surface(_surf);
    // gbm_surface_destroy(surf->nvgbm_surface);
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
#if 0
    NVGBM_FUNC(gbm_surface_create_with_modifiers)
    NVGBM_FUNC(gbm_surface_destroy)

    struct gbm_tudrm_device *dri = gbm_tudrm_device(gbm);
    struct gbm_tudrm_surface *surf;
    struct gbm_surface *nvgbm_surface;

    uint64_t *modifiers = calloc(count, sizeof(uint64_t));
    if (!modifiers) {
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        modifiers[i] = _modifiers[i] & ~NV_MODIFIER_MASK;
    }

    nvgbm_surface = gbm_surface_create_with_modifiers(dri->nvgbm_device, width, height, format, modifiers, count);
    if (!nvgbm_surface) {
        return NULL;
    }

    free(modifiers);

    surf = calloc(1, sizeof *surf);
    if (surf == NULL) {
        gbm_surface_destroy(nvgbm_surface);
        errno = ENOMEM;
        return NULL;
    }

    surf->nvgbm_surface = nvgbm_surface;
    return &surf->base;
#else

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

#endif
}

static void
gbm_tudrm_device_destroy(struct gbm_device *gbm)
{
    NVGBM_FUNC(gbm_device_destroy)
    struct gbm_tudrm_device *tudrm = gbm_tudrm_device(gbm);
    gbm_device_destroy(tudrm->nvgbm_device);
    free(tudrm);
}

static struct gbm_device *
gbm_tudrm_device_create(int fd, uint32_t gbm_backend_version)
{
    struct gbm_tudrm_device *tudrm;
    struct gbm_device *nvgbm_device;

    libnvgbm = dlopen("libnvgbm.so", RTLD_NOW);
    if (!libnvgbm) {
        errno = ENODEV;
        return NULL;
    }

    NVGBM_FUNC(gbm_create_device)
    NVGBM_FUNC(gbm_device_destroy)

    if (gbm_backend_version != GBM_BACKEND_ABI_VERSION) {
        errno = EINVAL;
        return NULL;
    }

    nvgbm_device = gbm_create_device(fd);
    if (!nvgbm_device) {
        return NULL;
    }

    tudrm = calloc(1, sizeof *tudrm);
    if (!tudrm) {
        gbm_device_destroy(nvgbm_device);
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

    tudrm->nvgbm_device = nvgbm_device;

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
