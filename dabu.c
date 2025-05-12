#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "lz4.h"

typedef struct {
    uint8_t *buffer;
    size_t cap;
    size_t size;
} string_T;

typedef struct {
    uint8_t *buffer;
    size_t size;
    size_t offset;
} block_T;

block_T*
block_create(const size_t size)
{
    if (size <= 0) return NULL;

    block_T *ptr = calloc(1, sizeof(block_T));
    if (!ptr)
    {
        fprintf(stderr, "calloc() failed when allocating for block_T");
        return NULL;
    }

    ptr->buffer = calloc(1, (sizeof(uint8_t) * size));

    ptr->size = size;
    ptr->offset = 0;

    return ptr;
}

void
block_reset(block_T *block)
{
    if (block)
    {
        block->offset = 0;
    }
}

void
block_free(block_T **block)
{
    if (block && *block)
    {
        if ((*block)->buffer)
        {
            free((*block)->buffer);
            (*block)->buffer = NULL;
        }

        free(*block);
        *block = NULL;
    }
}

void *
block_alloc(block_T *block, const size_t size)
{
    if ((block->offset + size) > block->size)
    {
        fprintf(stderr, "block_T out of memory!");
        return NULL;
    }

    void *ptr = block->buffer + block->offset;
    block->offset += size;

    return ptr;
}

string_T *
string_new(block_T *block, const char *text)
{
    if (!block || !text) return NULL;
    const size_t len = strlen(text) + 1;
    const size_t cap = len * sizeof(uint8_t);
    string_T *ptr = block_alloc(block, sizeof(string_T));

    assert(ptr);

    ptr->buffer = block_alloc(block, cap);
    memcpy(ptr->buffer, text, cap);
    ptr->size = ptr->cap = cap;

    return ptr;
}

string_T *
string_concat(block_T *block, const char *text, const char *extra)
{
    if (!block || !text || !extra) return NULL;
    const size_t text_len = strlen(text);
    const size_t extra_len = strlen(extra);
    const size_t len = text_len + extra_len + 1;
    const size_t cap = len * sizeof(uint8_t);
    string_T *ptr = block_alloc(block, sizeof(string_T));

    assert(ptr);

    ptr->buffer = block_alloc(block, cap);
    strncpy(ptr->buffer, text, text_len);
    strncat(ptr->buffer, extra, extra_len);
    ptr->buffer[len] = '\0';
    ptr->size = ptr->cap = cap;

    return ptr;
}


#define XABA_MAGIC 0x41424158
#define XALZ_MAGIC 0x5a4c4158

#pragma pack(push, 1)
typedef struct header_T {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_count;
    uint32_t index_entry_count;
    uint32_t index_size;
} header_T;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct descriptor_T {
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t debug_data_offset;
    uint32_t debug_data_size;
    uint32_t config_data_offset;
    uint32_t config_data_size;
} descriptor_T;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct hash_T {
    union {
        uint32_t hash32;
        uint64_t hash64;
    };
    uint32_t mapping_index;
    uint32_t local_store_index;
    uint32_t store_id;
} hash_T;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
        uint32_t magic;
        uint32_t index;
        uint32_t size;
} xalz_T;
#pragma pack(pop)

int
read_header(const FILE *file, header_T *header)
{

    if (file == NULL)
        return -1;

    fseek((FILE*)file, 0, SEEK_SET);

    if (fread(
            header,
            sizeof(struct header_T),
            1,
            (FILE*)file) != 1)
    {
        fprintf(stderr, "Failed reading file\n");
        return -1;
    }

    return 0;
}

hash_T*
get_hash(hash_T *list, size_t size, const uint32_t index)
{
    if (!list
            || size < 0
            || index > size)
        return NULL;

    hash_T *ptr = &list[index];
    return ptr;
}

descriptor_T*
get_descriptor(descriptor_T *list, size_t size, const uint32_t index)
{
    if (!list
            || size < 0
            || index > size)
        return NULL;

    descriptor_T *ptr = &list[index];
    return ptr;
}

const char*
change_file_ext(block_T *block, const char* path, const char* ext)
{
    if (!path || !ext) return NULL;
    const char* last_token = strrchr(path, '.');
    size_t ext_len = strlen(ext);
    if ((char)last_token[1] != 0x62) return NULL;

    size_t len = last_token - path;
    if (len <= 0) return NULL;

    char* new_path = block_alloc(block, len + ext_len + 1);
    if (!new_path) return NULL;

    strncpy(new_path, path, len);
    strncat(new_path, ext, ext_len);
    new_path[len + ext_len] = '\0';

    return new_path;
}

const char*
get_parent_dir(block_T *block, const char* path)
{
    if (!block || !path)  return NULL;

    const char* last_token = strrchr(path, '/');
    if (!last_token)
        last_token = strrchr(path, '\\');

    // TODO maybe return "."
    if (!last_token) return NULL;

    size_t len = (last_token - path) + 1; // Include slash within the string
    if (len <= 0) return NULL;

    char *dir = block_alloc(block, len + 1); // length + NULL
    if (!dir) return NULL;

    strncpy(dir, path, len);
    dir[len] = '\0';

    return dir;
}

const char*
get_dllname_from_manifest(block_T *block, const FILE* file, const uint64_t hash)
{
    uint32_t hash32 = 0;
    uint64_t hash64 = 0;
    uint32_t index = 0;
    uint32_t index2 = 0;
    char filename[4096] = { 0 };
    char buf[1024] = { 0 };

	rewind((FILE*)file);

	while (fgets(buf, sizeof(buf), (FILE*)file))
	{
		size_t ret = sscanf(buf, "0x%x  0x%llx  %d      %d      %s", &hash32, &hash64, &index, &index2, filename);
		if (ret == 5)
		{
			if (hash32 == (uint32_t)hash || hash64 == hash)
			{
                if (sizeof(filename) - (strlen(filename) + 5) >= 0)
                {
                    size_t len = strlen(filename);
                    filename[len] = '.';
                    filename[len+1] = 'd';
                    filename[len+2] = 'l';
                    filename[len+3] = 'l';
                    filename[len+4] = '\0';
                    char *underscore = strrchr(filename, '//');
                    if (underscore) underscore[0] = '_';
                    const size_t size = strlen(filename) + 1;
                    void *ptr = block_alloc(block, size);
                    if (!ptr)
                    {
                        fprintf(stderr, "block_alloc() failed\n");
                        return NULL;
                    }
                    memcpy(ptr, filename, size);
                    return ptr;
                }
			}
		}
	}

	return NULL;
}

int
write_file(const char *filename, uint8_t *data, size_t size)
{
    if (!filename || !data || size <= 0) return -1;
    FILE *file = fopen(filename, "wb");

    if (!file) return -1;

    int ret = fwrite(data, sizeof(uint8_t), size, file);

    fclose(file);

    return ret;
}

bool
assemblies_dump(const char *path, const bool dump)
{
    if (path == NULL)
        return false;

    FILE *file = fopen(path, "rb");

    if (file == NULL)
    {
        fprintf(stderr, "Failed opening assemblies blob file\n");
		return false;
    }

	bool ret = true;

    header_T header = { 0 };

    int r = read_header(file, &header);

    if (r < 0)
    {
        fprintf(stderr, "Failed reading file\n");
		ret = false;
		goto EXIT;
    }

    if (header.magic != XABA_MAGIC)
    {
        fprintf(stderr, "This is not a AssemblyStore File\n");
		ret = false;
		goto EXIT;
    }

    if (header.magic != XABA_MAGIC)
    {
        fprintf(stderr, "This is not a AssemblyStore File\n");
		ret = false;
		goto EXIT;
    }

    assert(header.entry_count > 0);

    //TODO improve mem allocation calculation
    block_T *block = block_create(header.entry_count * (1024 * 1024));

    if (!block)
    {
        fprintf(stderr, "block_create() failed\n");
		ret = false;
		goto EXIT;
    }

    assert(header.entry_count > 0);

    const char *manifest_path = change_file_ext(block, path, ".manifest");

    FILE *manifest = fopen(manifest_path, "r");
    if (manifest == NULL)
    {
        fprintf(stderr, "Failed opening manifest file\n");
		ret = false;
		goto EXIT;
    }

    descriptor_T* descriptors = block_alloc(block, header.entry_count * sizeof(descriptor_T));

    if (!descriptors)
    {
        fprintf(stderr, "malloc failed");
		ret = false;
		goto EXIT;
    }

    fprintf(stdout, "magic: 0x%X, version: 0x%u, entries: %u, index_entries: %u, index_size: %u \n",
            header.magic, header.version, header.entry_count, header.index_entry_count, header.index_size);

    for (size_t i = 0; i < header.entry_count; i++)
    {
        if (fread(
                &descriptors[i],
                sizeof(descriptor_T),
                1,
                file) != 1)
        {
            fprintf(stderr, "Failed reading file\n");
			ret = false;
            goto EXIT;
        }
    }

    size_t fpos = ftell(file);

    size_t count = header.index_entry_count;
    assert(count > 0);

    hash_T *hash32list =  block_alloc(block, sizeof(hash_T) * count);

    for (size_t i = 0; i < header.index_entry_count; i++)
    {
        if (fread(
                &hash32list[i],
                sizeof(hash_T),
                1,
                file) != 1)
        {
            fprintf(stderr, "Failed reading file\n");
			ret = false;
            goto EXIT;
        }
    }

    hash_T *hash64list =  block_alloc(block, sizeof(hash_T) * count);

    for (size_t i = 0; i < header.index_entry_count; i++)
    {
        if (fread(
                &hash64list[i],
                sizeof(hash_T),
                1,
                file) != 1)
        {
            fprintf(stderr, "Failed reading file\n");
			ret = false;
            goto EXIT;
        }
    }

    fpos = ftell(file);

    for (size_t i = 0; i < header.index_entry_count; i++)
    {
        hash_T* hash = get_hash(hash32list, count, i);
        descriptor_T* dsc = get_descriptor(descriptors, count, hash->local_store_index);
		const char* dllname = get_dllname_from_manifest(block, manifest, hash->hash32);

        fseek((FILE*)file, dsc->data_offset, SEEK_SET);
        fpos = ftell(file);

        xalz_T xalz = { 0 };
        if (fread(
                &xalz,
                sizeof(xalz_T),
                1,
                file) != 1)
        {
            fprintf(stderr, "Failed reading file\n");
			ret = false;
            goto EXIT;
        }

		fprintf(stdout, "file: %s pos: 0x%zx index: %ld magic: 0x%lx xalz.size: 0x%lx data_offset: 0x%lx data_size: %ld\n",
                dllname, fpos, hash->local_store_index, xalz.magic, xalz.size, dsc->data_offset, dsc->data_size);
		assert(xalz.magic == 0x5a4c4158);
        assert(xalz.size > 0);

        size_t compressed_file_size = dsc->data_size;
        uint8_t* compressed_payload = block_alloc(block, compressed_file_size);
        if (!compressed_payload)
        {
            fprintf(stderr, "malloc failed\n");
			ret = false;
            goto EXIT;
        }

        if (fread(
                compressed_payload,
                compressed_file_size,
                1,
                file) < 1)
        {
			if(!feof(file))
			{
            	fprintf(stderr, "Failed reading file 2\n");
				ret = false;
            	goto EXIT;
			}
        }

        size_t data_size = xalz.size;
        uint8_t *data = block_alloc(block, data_size);
        if (!data)
        {
            fprintf(stderr, "malloc failed\n");
			ret = false;
            goto EXIT;
        }

        int ret =  LZ4_decompress_fast(compressed_payload, data, data_size);
        if (ret <= 0)
        {
            fprintf(stderr, "LZ4 decompression failed\n");
			ret = false;
            goto EXIT;
        }

        const char *dir = get_parent_dir(block, path);
        string_T *output = (dir) ? string_concat(block, dir, dllname) : string_new(block, dllname);

        if (!output)
        {
            fprintf(stderr, "string operation failed\n");
			ret = false;
            goto EXIT;
        }

        if (dump)
        {
            ret = write_file(output->buffer, data, data_size);
            if (ret <= 0)
            {
                fprintf(stderr, "write_file() failed\n");
				ret = false;
                goto EXIT;
            }
        }

    }

EXIT:
    if (file)
        fclose(file);

    if (manifest)
        fclose(manifest);

    block_free(&block);

	return ret;
}

