//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	WAD I/O functions.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"

typedef struct
{
    wad_file_t wad;
    FILE *fstream;
#ifdef __3DS__
    byte *data; // the whole file, or NULL
#endif
} stdc_wad_file_t;


static wad_file_t *W_StdC_OpenFile(const char *path)
{
    stdc_wad_file_t *result;
    FILE *fstream;

    fstream = M_fopen(path, "rb");

    if (fstream == NULL)
    {
        return NULL;
    }

    // Create a new stdc_wad_file_t to hold the file handle.

    result = Z_Malloc(sizeof(stdc_wad_file_t), PU_STATIC, 0);
    result->wad.file_class = &stdc_wad_file;
    result->wad.mapped = NULL;
    result->wad.length = M_FileLength(fstream);
    result->wad.path = M_StringDuplicate(path);
    result->fstream = fstream;

#ifdef __3DS__
    // [3DS] The whole file is read into memory at once and the lumps are
    // copied from there: reading hundreds of small lumps from the SD card
    // made the startup take seconds. (Not used in place as a mapped file:
    // lumps are not aligned, and ARM faults on some unaligned loads.) Only
    // while enough memory stays free; otherwise they are read from the file.
    {
        byte *data = malloc(result->wad.length);
        void *headroom = malloc(8 * 1024 * 1024);

        result->data = NULL;

        if (data != NULL && headroom != NULL)
        {
            setvbuf(fstream, NULL, _IONBF, 0);

            if (fread(data, 1, result->wad.length, fstream) == result->wad.length)
            {
                result->data = data;
                data = NULL;
            }
        }

        free(headroom);
        free(data);
        printf(" %s %s\n", path, result->data != NULL ? "read into memory" : "read from the SD card");
    }
#endif

    return &result->wad;
}

static void W_StdC_CloseFile(wad_file_t *wad)
{
    stdc_wad_file_t *stdc_wad;

    stdc_wad = (stdc_wad_file_t *) wad;

#ifdef __3DS__
    free(stdc_wad->data);
#endif
    fclose(stdc_wad->fstream);
    Z_Free(stdc_wad);
}

// Read data from the specified position in the file into the 
// provided buffer.  Returns the number of bytes read.

size_t W_StdC_Read(wad_file_t *wad, unsigned int offset,
                   void *buffer, size_t buffer_len)
{
    stdc_wad_file_t *stdc_wad;
    size_t result;

    stdc_wad = (stdc_wad_file_t *) wad;

#ifdef __3DS__
    if (stdc_wad->data != NULL)
    {
        if (offset >= wad->length)
        {
            return 0;
        }

        if (buffer_len > wad->length - offset)
        {
            buffer_len = wad->length - offset;
        }

        memcpy(buffer, stdc_wad->data + offset, buffer_len);
        return buffer_len;
    }
#endif

    // Jump to the specified position in the file.

    fseek(stdc_wad->fstream, offset, SEEK_SET);

    // Read into the buffer.

    result = fread(buffer, 1, buffer_len, stdc_wad->fstream);

    return result;
}


wad_file_class_t stdc_wad_file = 
{
    W_StdC_OpenFile,
    W_StdC_CloseFile,
    W_StdC_Read,
};


