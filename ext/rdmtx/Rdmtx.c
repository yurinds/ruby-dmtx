/*
Rdmtx - Ruby wrapper for libdmtx

Copyright (C) 2008 Romain Goyet

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

Contact: r.goyet@gmail.com
*/

#include <ruby.h>
#include <ruby/encoding.h>
#include <dmtx.h>
#include <png.h>
#include <stdint.h>
#include <limits.h>

/* Constants */
#define DEFAULT_MARGIN 5
#define DEFAULT_MODULE 5
#define MAX_MARGIN 1000
#define MAX_MODULE 100
#define MAX_IMAGE_DIMENSION 10000
#define PNG_COMPRESSION_LEVEL 6

/* Global Ruby class references */
static VALUE cRdmtx;
static VALUE cRdmtxImage;

/* Cached method IDs for performance */
static ID id_binwrite;

/* Structure for PNG writing callback */
typedef struct {
    VALUE str;
} PngStringWriter;

/* Structure for resource cleanup */
typedef struct {
    DmtxEncode *enc;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep *row_pointers;
    int cleanup_stage; /* 0=none, 1=enc, 2=png, 3=info, 4=rows */
} EncodeResources;

/* PNG write callback - writes to Ruby string */
static void png_write_callback(png_structp png_ptr, png_bytep data, png_size_t length) {
    PngStringWriter *writer = (PngStringWriter *)png_get_io_ptr(png_ptr);
    if (writer == NULL || writer->str == Qnil) {
        png_error(png_ptr, "Invalid writer structure");
        return;
    }

    /* rb_str_cat can raise exception, but we're inside libpng callback
     * This is handled by setjmp in the main function */
    rb_str_cat(writer->str, (const char *)data, length);
}

/* Cleanup function for rb_ensure */
static VALUE encode_cleanup(VALUE arg) {
    EncodeResources *res = (EncodeResources *)arg;

    if (res->cleanup_stage >= 4 && res->row_pointers != NULL) {
        xfree(res->row_pointers);
        res->row_pointers = NULL;
    }

    if (res->cleanup_stage >= 3 && res->info_ptr != NULL) {
        png_destroy_info_struct(res->png_ptr, &res->info_ptr);
        res->info_ptr = NULL;
    }

    if (res->cleanup_stage >= 2 && res->png_ptr != NULL) {
        png_destroy_write_struct(&res->png_ptr, NULL);
        res->png_ptr = NULL;
    }

    if (res->cleanup_stage >= 1 && res->enc != NULL) {
        dmtxEncodeDestroy(&res->enc);
        res->enc = NULL;
    }

    return Qnil;
}

/* Check for integer overflow in multiplication */
static int check_mul_overflow(size_t a, size_t b, size_t *result) {
    if (a == 0 || b == 0) {
        *result = 0;
        return 0;
    }

    if (a > SIZE_MAX / b) {
        return 1; /* Overflow */
    }

    *result = a * b;
    return 0;
}

/* Validate input parameters */
static int validate_params(int margin, int module, const char *str, size_t str_len) {
    if (margin < 0 || margin > MAX_MARGIN) {
        rb_raise(rb_eArgError, "margin must be between 0 and %d", MAX_MARGIN);
        return 0;
    }

    if (module < 0 || module > MAX_MODULE) {
        rb_raise(rb_eArgError, "module must be between 0 and %d", MAX_MODULE);
        return 0;
    }

    if (str == NULL || str_len == 0) {
        rb_raise(rb_eArgError, "string cannot be empty");
        return 0;
    }

    if (str_len > UINT_MAX) {
        rb_raise(rb_eArgError, "string too long");
        return 0;
    }

    return 1;
}

/* Main encoding function body (for rb_ensure) */
static VALUE rdmtx_encode_body(VALUE arg) {
    EncodeResources *res = (EncodeResources *)arg;
    VALUE *args = (VALUE *)res->enc; /* Temporarily stored args here */
    VALUE string = args[0];
    VALUE margin = args[1];
    VALUE module = args[2];
    VALUE size = args[3];

    VALUE safeString;
    VALUE pngString;
    int safeMargin, safeModule, safeSize;
    int width, height;
    int row_stride;
    size_t row_offset;
    size_t estimated_png_size;
    int y;
    PngStringWriter writer;
    DmtxEncode *enc;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep *row_pointers = NULL;

    /* Convert and validate parameters */
    safeString = StringValue(string);
    safeMargin = NIL_P(margin) ? DEFAULT_MARGIN : NUM2INT(margin);
    safeModule = NIL_P(module) ? DEFAULT_MODULE : NUM2INT(module);
    safeSize = NIL_P(size) ? DmtxSymbolSquareAuto : NUM2INT(size);

    if (!validate_params(safeMargin, safeModule, RSTRING_PTR(safeString),
                        RSTRING_LEN(safeString))) {
        return Qnil;
    }

    /* Create libdmtx encoder */
    enc = dmtxEncodeCreate();
    if (enc == NULL) {
        rb_raise(rb_eRuntimeError, "Failed to create dmtx encoder");
        return Qnil;
    }

    res->enc = enc;
    res->cleanup_stage = 1;

    /* Configure encoder */
    if (dmtxEncodeSetProp(enc, DmtxPropPixelPacking, DmtxPack24bppRGB) == DmtxFail ||
        dmtxEncodeSetProp(enc, DmtxPropSizeRequest, safeSize) == DmtxFail ||
        dmtxEncodeSetProp(enc, DmtxPropMarginSize, safeMargin) == DmtxFail ||
        dmtxEncodeSetProp(enc, DmtxPropModuleSize, safeModule) == DmtxFail) {
        rb_raise(rb_eRuntimeError, "Failed to configure dmtx encoder");
        return Qnil;
    }

    /* Encode barcode */
    if (dmtxEncodeDataMatrix(enc, RSTRING_LEN(safeString),
            (unsigned char *)RSTRING_PTR(safeString)) == DmtxFail) {
        rb_raise(rb_eRuntimeError, "Failed to encode data matrix");
        return Qnil;
    }

    /* Get image dimensions and validate */
    width = dmtxImageGetProp(enc->image, DmtxPropWidth);
    height = dmtxImageGetProp(enc->image, DmtxPropHeight);

    if (width <= 0 || height <= 0 ||
        width > MAX_IMAGE_DIMENSION || height > MAX_IMAGE_DIMENSION) {
        rb_raise(rb_eRuntimeError, "Invalid image dimensions: %dx%d", width, height);
        return Qnil;
    }

    /* Pre-allocate string with estimated size to avoid multiple reallocs */
    if (check_mul_overflow(width, height, &estimated_png_size) ||
        check_mul_overflow(estimated_png_size, 3, &estimated_png_size)) {
        rb_raise(rb_eRuntimeError, "Image too large");
        return Qnil;
    }
    estimated_png_size = estimated_png_size * 11 / 10 + 1024; /* +10% overhead + headers */

    pngString = rb_str_buf_new(estimated_png_size);
    rb_enc_associate(pngString, rb_ascii8bit_encoding());

    /* Create PNG structures */
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        rb_raise(rb_eRuntimeError, "Failed to create PNG write struct");
        return Qnil;
    }

    res->png_ptr = png_ptr;
    res->cleanup_stage = 2;

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        rb_raise(rb_eRuntimeError, "Failed to create PNG info struct");
        return Qnil;
    }

    res->info_ptr = info_ptr;
    res->cleanup_stage = 3;

    /* Set up error handling for libpng */
    if (setjmp(png_jmpbuf(png_ptr))) {
        /* libpng encountered an error */
        rb_raise(rb_eRuntimeError, "PNG encoding error");
        return Qnil;
    }

    /* Configure PNG output */
    writer.str = pngString;
    png_set_write_fn(png_ptr, &writer, png_write_callback, NULL);
    png_set_compression_level(png_ptr, PNG_COMPRESSION_LEVEL);

    png_set_IHDR(png_ptr, info_ptr,
                 width, height,
                 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    /* Prepare row pointers */
    row_stride = dmtxImageGetProp(enc->image, DmtxPropRowSizeBytes);
    if (row_stride <= 0) {
        rb_raise(rb_eRuntimeError, "Invalid row stride");
        return Qnil;
    }

    row_pointers = ALLOC_N(png_bytep, height);
    res->row_pointers = row_pointers;
    res->cleanup_stage = 4;

    /* Build row pointers with overflow checking */
    for (y = 0; y < height; y++) {
        if (check_mul_overflow((size_t)y, (size_t)row_stride, &row_offset)) {
            rb_raise(rb_eRuntimeError, "Row offset overflow at row %d", y);
            return Qnil;
        }
        row_pointers[y] = (png_bytep)(enc->image->pxl + row_offset);
    }

    /* Write PNG image */
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, NULL);

    /* Cleanup will happen in encode_cleanup via rb_ensure */
    return pngString;
}

/* Ruby method: Rdmtx#initialize */
static VALUE rdmtx_init(VALUE self) {
    return self;
}

/* Ruby method: Rdmtx#encode */
static VALUE rdmtx_encode(int argc, VALUE *argv, VALUE self) {
    VALUE args[4];
    EncodeResources res;
    VALUE result;

    rb_scan_args(argc, argv, "13", &args[0], &args[1], &args[2], &args[3]);

    /* Initialize resource structure */
    res.enc = (DmtxEncode *)args; /* Temporarily store args */
    res.png_ptr = NULL;
    res.info_ptr = NULL;
    res.row_pointers = NULL;
    res.cleanup_stage = 0;

    /* Execute with guaranteed cleanup */
    result = rb_ensure(rdmtx_encode_body, (VALUE)&res,
                      encode_cleanup, (VALUE)&res);

    if (result == Qnil) {
        return Qnil;
    }

    /* Create and return Rdmtx::Image instance */
    return rb_class_new_instance(1, &result, cRdmtxImage);
}

/* Ruby method: Rdmtx::Image#initialize */
static VALUE rdmtx_image_init(VALUE self, VALUE data) {
    rb_iv_set(self, "@data", data);
    return self;
}

/* Ruby method: Rdmtx::Image#write */
static VALUE rdmtx_image_write(VALUE self, VALUE path) {
    VALUE data = rb_iv_get(self, "@data");

    /* Use cached File class and binwrite method ID */
    rb_funcall(rb_cFile, id_binwrite, 2, path, data);
    return path;
}

/* Ruby method: Rdmtx::Image#data */
static VALUE rdmtx_image_data(VALUE self) {
    return rb_iv_get(self, "@data");
}

/* Module initialization */
void Init_Rdmtx(void) {
    /* Cache frequently used method IDs for performance */
    id_binwrite = rb_intern("binwrite");

    /* Define Rdmtx class */
    cRdmtx = rb_define_class("Rdmtx", rb_cObject);
    rb_define_method(cRdmtx, "initialize", rdmtx_init, 0);
    rb_define_method(cRdmtx, "encode", rdmtx_encode, -1);

    /* Define Rdmtx::Image class */
    cRdmtxImage = rb_define_class_under(cRdmtx, "Image", rb_cObject);
    rb_define_method(cRdmtxImage, "initialize", rdmtx_image_init, 1);
    rb_define_method(cRdmtxImage, "write", rdmtx_image_write, 1);
    rb_define_method(cRdmtxImage, "data", rdmtx_image_data, 0);

    /* Define size constants */
    rb_define_global_const("DmtxSymbolRectAuto", INT2FIX(DmtxSymbolRectAuto));
    rb_define_global_const("DmtxSymbolSquareAuto", INT2FIX(DmtxSymbolSquareAuto));
    rb_define_global_const("DmtxSymbolShapeAuto", INT2FIX(DmtxSymbolShapeAuto));

    /* Square symbols */
    rb_define_global_const("DmtxSymbol10x10", INT2FIX(DmtxSymbol10x10));
    rb_define_global_const("DmtxSymbol12x12", INT2FIX(DmtxSymbol12x12));
    rb_define_global_const("DmtxSymbol14x14", INT2FIX(DmtxSymbol14x14));
    rb_define_global_const("DmtxSymbol16x16", INT2FIX(DmtxSymbol16x16));
    rb_define_global_const("DmtxSymbol18x18", INT2FIX(DmtxSymbol18x18));
    rb_define_global_const("DmtxSymbol20x20", INT2FIX(DmtxSymbol20x20));
    rb_define_global_const("DmtxSymbol22x22", INT2FIX(DmtxSymbol22x22));
    rb_define_global_const("DmtxSymbol24x24", INT2FIX(DmtxSymbol24x24));
    rb_define_global_const("DmtxSymbol26x26", INT2FIX(DmtxSymbol26x26));
    rb_define_global_const("DmtxSymbol32x32", INT2FIX(DmtxSymbol32x32));
    rb_define_global_const("DmtxSymbol36x36", INT2FIX(DmtxSymbol36x36));
    rb_define_global_const("DmtxSymbol40x40", INT2FIX(DmtxSymbol40x40));
    rb_define_global_const("DmtxSymbol44x44", INT2FIX(DmtxSymbol44x44));
    rb_define_global_const("DmtxSymbol48x48", INT2FIX(DmtxSymbol48x48));
    rb_define_global_const("DmtxSymbol52x52", INT2FIX(DmtxSymbol52x52));
    rb_define_global_const("DmtxSymbol64x64", INT2FIX(DmtxSymbol64x64));
    rb_define_global_const("DmtxSymbol72x72", INT2FIX(DmtxSymbol72x72));
    rb_define_global_const("DmtxSymbol80x80", INT2FIX(DmtxSymbol80x80));
    rb_define_global_const("DmtxSymbol88x88", INT2FIX(DmtxSymbol88x88));
    rb_define_global_const("DmtxSymbol96x96", INT2FIX(DmtxSymbol96x96));
    rb_define_global_const("DmtxSymbol104x104", INT2FIX(DmtxSymbol104x104));
    rb_define_global_const("DmtxSymbol120x120", INT2FIX(DmtxSymbol120x120));
    rb_define_global_const("DmtxSymbol132x132", INT2FIX(DmtxSymbol132x132));
    rb_define_global_const("DmtxSymbol144x144", INT2FIX(DmtxSymbol144x144));

    /* Rectangular symbols */
    rb_define_global_const("DmtxSymbol8x18", INT2FIX(DmtxSymbol8x18));
    rb_define_global_const("DmtxSymbol8x32", INT2FIX(DmtxSymbol8x32));
    rb_define_global_const("DmtxSymbol12x26", INT2FIX(DmtxSymbol12x26));
    rb_define_global_const("DmtxSymbol12x36", INT2FIX(DmtxSymbol12x36));
    rb_define_global_const("DmtxSymbol16x36", INT2FIX(DmtxSymbol16x36));
    rb_define_global_const("DmtxSymbol16x48", INT2FIX(DmtxSymbol16x48));
}
