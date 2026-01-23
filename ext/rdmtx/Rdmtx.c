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

/* $Id:$ */

#include <ruby.h>
#include <dmtx.h>
#include <png.h>

VALUE cRdmtxImage;

static VALUE rdmtx_init(VALUE self) {
    return self;
}

typedef struct {
    VALUE str;
} PngStringWriter;

static void png_write_callback(png_structp png_ptr, png_bytep data, png_size_t length) {
    PngStringWriter *writer = (PngStringWriter *)png_get_io_ptr(png_ptr);
    rb_str_cat(writer->str, (const char *)data, length);
}

static VALUE rdmtx_image_init(VALUE self, VALUE data) {
    rb_iv_set(self, "@data", data);
    return self;
}

static VALUE rdmtx_image_write(VALUE self, VALUE path) {
    VALUE data = rb_iv_get(self, "@data");
    VALUE file_class = rb_const_get(rb_cObject, rb_intern("File"));
    rb_funcall(file_class, rb_intern("binwrite"), 2, path, data);
    return path;
}

static VALUE rdmtx_image_data(VALUE self) {
    return rb_iv_get(self, "@data");
}

static VALUE rdmtx_encode(int argc, VALUE * argv, VALUE self) {

    VALUE string, margin, module, size;
    VALUE safeString;
    VALUE outputImage;
    VALUE pngString;

    int safeMargin, safeModule, safeSize;
    int width;
    int height;
    DmtxEncode * enc;

    rb_scan_args(argc, argv, "13", &string,
                 &margin, &module, &size);

    safeString = StringValue(string);
    if(NIL_P(margin)) {
        safeMargin = 5;
    } else {
        safeMargin = NUM2INT(margin);
    }
    if(NIL_P(module)) {
        safeModule = 5;
    } else {
        safeModule = NUM2INT(module);
    }
    if(NIL_P(size)) {
        safeSize = DmtxSymbolSquareAuto;
    } else {
        safeSize = NUM2INT(size);
    }

    // printf("Margin = %d, Module = %d, Size = %d\n", safeMargin, safeModule, safeSize);

    /* Create and initialize libdmtx structures */
    enc = dmtxEncodeCreate();

    dmtxEncodeSetProp(enc, DmtxPropPixelPacking, DmtxPack24bppRGB);
    dmtxEncodeSetProp(enc, DmtxPropSizeRequest, safeSize);

    dmtxEncodeSetProp(enc, DmtxPropMarginSize, safeMargin);
    dmtxEncodeSetProp(enc, DmtxPropModuleSize, safeModule);
    dmtxEncodeSetProp(enc, DmtxPropFnc1, 35);

    /* Create barcode image */
    if (dmtxEncodeDataMatrix(enc, RSTRING_LEN(safeString),
            (unsigned char *)RSTRING_PTR(safeString)) == DmtxFail) {
//        printf("Fatal error !\n");
        dmtxEncodeDestroy(&enc);
        return Qnil;
    }

    width = dmtxImageGetProp(enc->image, DmtxPropWidth);
    height = dmtxImageGetProp(enc->image, DmtxPropHeight);

    pngString = rb_str_new("", 0);
    {
        png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        png_infop info_ptr;
        PngStringWriter writer;
        png_bytep *row_pointers = NULL;
        int y;

        if (png_ptr == NULL) {
            dmtxEncodeDestroy(&enc);
            return Qnil;
        }

        info_ptr = png_create_info_struct(png_ptr);
        if (info_ptr == NULL) {
            png_destroy_write_struct(&png_ptr, NULL);
            dmtxEncodeDestroy(&enc);
            return Qnil;
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
            if (row_pointers != NULL) {
                xfree(row_pointers);
            }
            png_destroy_write_struct(&png_ptr, &info_ptr);
            dmtxEncodeDestroy(&enc);
            return Qnil;
        }

        writer.str = pngString;
        png_set_write_fn(png_ptr, &writer, png_write_callback, NULL);

        png_set_IHDR(png_ptr, info_ptr,
                     width, height,
                     8, PNG_COLOR_TYPE_RGB,
                     PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_DEFAULT,
                     PNG_FILTER_TYPE_DEFAULT);

        png_write_info(png_ptr, info_ptr);

        row_pointers = ALLOC_N(png_bytep, height);
        for (y = 0; y < height; y++) {
            row_pointers[y] = (png_bytep)(enc->image->pxl + (size_t)y * width * 3);
        }

        png_write_image(png_ptr, row_pointers);
        png_write_end(png_ptr, NULL);

        xfree(row_pointers);
        png_destroy_write_struct(&png_ptr, &info_ptr);
    }

    /* Clean up */
    dmtxEncodeDestroy(&enc);

    outputImage = rb_class_new_instance(1, &pngString, cRdmtxImage);
    return outputImage;
}

VALUE cRdmtx;
void Init_Rdmtx(void) {
    cRdmtx = rb_define_class("Rdmtx", rb_cObject);
    rb_define_method(cRdmtx, "initialize", rdmtx_init, 0);
    rb_define_method(cRdmtx, "encode", rdmtx_encode, -1);

    cRdmtxImage = rb_define_class_under(cRdmtx, "Image", rb_cObject);
    rb_define_method(cRdmtxImage, "initialize", rdmtx_image_init, 1);
    rb_define_method(cRdmtxImage, "write", rdmtx_image_write, 1);
    rb_define_method(cRdmtxImage, "data", rdmtx_image_data, 0);

    rb_define_global_const("DmtxSymbolRectAuto", INT2FIX(DmtxSymbolRectAuto));
    rb_define_global_const("DmtxSymbolSquareAuto", INT2FIX(DmtxSymbolSquareAuto));
    rb_define_global_const("DmtxSymbolShapeAuto", INT2FIX(DmtxSymbolShapeAuto));

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
    rb_define_global_const("DmtxSymbol8x18", INT2FIX(DmtxSymbol8x18));
    rb_define_global_const("DmtxSymbol8x32", INT2FIX(DmtxSymbol8x32));
    rb_define_global_const("DmtxSymbol12x26", INT2FIX(DmtxSymbol12x26));
    rb_define_global_const("DmtxSymbol12x36", INT2FIX(DmtxSymbol12x36));
    rb_define_global_const("DmtxSymbol16x36", INT2FIX(DmtxSymbol16x36));
    rb_define_global_const("DmtxSymbol16x48", INT2FIX(DmtxSymbol16x48));
}
