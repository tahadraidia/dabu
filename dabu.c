#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "lz4.h"

#include "dabu.h"

#define debug_env "dabu_debug"
#define is_debug (getenv(debug_env) != NULL)

typedef struct {
    char *buffer;
    size_t cap;
    size_t size;
} string_T;

struct block_T {
    int8_t *buffer;
    size_t size;
    size_t offset;
};

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
        fprintf(stderr,"block_T out of memory! current block size: 0x%lx requested size: 0x%lx\n", block->size, size);
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

    if (!ptr)
    {
	    fprintf(stderr, "block_alloc() failed - file:%s:%d\n", __FILE__, __LINE__);
	    return NULL;
    }

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

    if (!ptr)
    {
	    fprintf(stderr, "block_alloc() failed - file:%s:%d\n", __FILE__, __LINE__);
	    return NULL;
    }

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

void
list_init(block_T **block, assembly_T **list, const size_t size)
{
    *list = block_alloc(*block, size);
    if (list && *list) (*list)->next = NULL;
}

void
list_append(
	block_T **block,
	assembly_T **list,
	const char *name,
	const size_t size)
{
    assembly_T *iter = *list;

    while (iter && iter->next)
        iter = iter->next;

    assembly_T *new_node = block_alloc(*block, sizeof(assembly_T));
    if (!new_node)
        return;

    new_node->next = NULL;
    memcpy(new_node->name, name, MAX_NAME);
    new_node->size = size;

    if (iter)
        iter->next = new_node;
    else
        *list = new_node;
}

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
        fprintf(stderr, "fread() failed fie:%s:%d\n", __FILE__, __LINE__);
        return -1;
    }

    return 0;
}

hash_T*
get_hash(hash_T *list, const size_t size, const size_t index)
{
    if (!list
            || index > size)
        return NULL;

    hash_T *ptr = &list[index];
    return ptr;
}

descriptor_T*
get_descriptor(descriptor_T *list, size_t size, const uint32_t index)
{
    if (!list
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
    if (!last_token) return NULL;

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

char*
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
		size_t ret = sscanf(buf, "0x%x  0x%lx  %d      %d      %s", &hash32, &hash64, &index, &index2, filename);
		if (ret == 5)
		{
			if (hash32 == (uint32_t)hash || hash64 == hash)
			{
                if ((strlen(filename) + 5) <= sizeof(filename))
                {
                    size_t len = strlen(filename);
                    filename[len] = '.';
                    filename[len+1] = 'd';
                    filename[len+2] = 'l';
                    filename[len+3] = 'l';
                    filename[len+4] = '\0';
                    char *underscore = strrchr(filename, '/');
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

size_t
write_file(const char *filename, char *data, size_t size)
{
    if (!filename || !data || size <= 0) return -1;
    FILE *file = fopen(filename, "wb");

    if (!file) return -1;

    size_t ret = fwrite(data, sizeof(char), size, file);

    fclose(file);

    return ret;
}

size_t
assemblies_dump(
	block_T **block,
	const char *path,
	assembly_T **list,
	const bool dump)
{
    if (path == NULL || *path == '\0' || strlen(path) <= 0)
    {
	printf("received invalid parameter\n");
        return 0;
    }

    FILE *file = NULL;
    FILE *manifest = NULL;
    bool has_manifest = true;

    file = fopen(path, "rb");

    if (file == NULL)
    {
        fprintf(stderr, "Failed opening assemblies blob file\n");
		return 0;
    }

	size_t count = 0;

    header_T header = { 0 };

    int r = read_header(file, &header);

    if (r < 0)
    {
        fprintf(stderr, "Failed reading file\n");
		goto EXIT;
    }

    if (header.magic != XABA_MAGIC)
    {
        fprintf(stderr, "%s is not a AssemblyStore File\n", path);
		goto EXIT;
    }

    if (header.magic != XABA_MAGIC)
    {
        fprintf(stderr, "This is not a AssemblyStore File\n");
		goto EXIT;
    }

    if(header.entry_count <= 0)
    {
        fprintf(stderr, "received a non-valid entry count\n");
		goto EXIT;
    }

    //TODO improve mem allocation calculation
    *block = block_create((header.entry_count * (1024 * 1024)) + (sizeof(assembly_T) * header.entry_count));

    if (!block)
    {
        fprintf(stderr, "block_create() failed\n");
		goto EXIT;
    }

    //assert(header.entry_count > 0);

    list_init(block, list, sizeof(assembly_T));

    const char *manifest_path = change_file_ext(*block, path, ".manifest");

    manifest = fopen(manifest_path, "r");
    if (manifest == NULL)
    {
        fprintf(stderr, "Failed opening manifest file\n");
	has_manifest = false;
    }

    descriptor_T* descriptors = block_alloc(*block, header.entry_count * sizeof(descriptor_T));
    if (!descriptors)
    {
        fprintf(stderr, "block_alloc() failed: %s:%d\n", __FILE__, __LINE__);
	goto EXIT;
    }

    if (is_debug)
    {
        fprintf(stdout, "magic: 0x%X, version: 0x%u, entries: %u, index_entries: %u, index_size: %u \n",
                header.magic, header.version, header.entry_count, header.index_entry_count, header.index_size);
    }

    for (size_t i = 0; i < header.entry_count; i++)
    {
        if (fread(
                &descriptors[i],
                sizeof(descriptor_T),
                1,
                file) != 1)
        {
            fprintf(stderr, "fread() failed fie:%s:%d\n", __FILE__, __LINE__);
            goto EXIT;
        }
    }

    size_t fpos = ftell(file);

    count = header.index_entry_count;
    if (count <= 0)
    {
        fprintf(stderr, "received a non-valid index entry count\n");
		goto EXIT;
    }

    hash_T *hash32list =  block_alloc(*block, sizeof(hash_T) * count);
    if(hash32list == NULL)
    {
	    fprintf(stderr, "block_alloc() failed file:%s:%d\n", __FILE__, __LINE__);
            goto EXIT;
    }

    for (size_t i = 0; i < header.index_entry_count; i++)
    {
        if (fread(
                &hash32list[i],
                sizeof(hash_T),
                1,
                file) != 1)
        {
            fprintf(stderr, "fread() failed fie:%s:%d\n", __FILE__, __LINE__);
            goto EXIT;
        }
    }

    hash_T *hash64list =  block_alloc(*block, sizeof(hash_T) * count);
    if(hash64list == NULL)
    {
	    fprintf(stderr, "block_alloc() failed file:%s:%d\n", __FILE__, __LINE__);
            goto EXIT;
    }

    for (size_t i = 0; i < header.index_entry_count; i++)
    {
        if (fread(
                &hash64list[i],
                sizeof(hash_T),
                1,
                file) != 1)
        {
            fprintf(stderr, "fread() failed fie:%s:%d\n", __FILE__, __LINE__);
            goto EXIT;
        }
    }

    fpos = ftell(file);

    for (size_t i = 0; i < header.index_entry_count; i++)
    {
        hash_T* hash = get_hash(hash32list, count, i);
	if (!hash)
	{
		fprintf(stderr, "Failed getting hash object for index 0x%lx\n", i);
		continue;
	}
        descriptor_T* dsc = get_descriptor(descriptors, count, hash->local_store_index);
	if (!dsc)
	{
		fprintf(stderr, "Failed getting descriptor object for local store index 0x%x\n", hash->local_store_index);
		continue;
	}

	char* dllname = NULL;
	char hexdllname[16] = { 0 };
	sprintf(hexdllname, "0x%x.dll", hash->hash32);

	if (has_manifest)
		dllname = get_dllname_from_manifest(*block, manifest, hash->hash32);
	else
		dllname = (char*)&hexdllname;


        fseek((FILE*)file, dsc->data_offset, SEEK_SET);
        fpos = ftell(file);

        xalz_T xalz = { 0 };
        if (fread(
                &xalz,
                sizeof(xalz_T),
                1,
                file) != 1)
        {
            fprintf(stderr, "fread() failed fie:%s:%d\n", __FILE__, __LINE__);
	    continue;
        }

        if (is_debug)
        {
            fprintf(stdout, "file: %s pos: 0x%zx index: %d magic: 0x%x xalz.size: 0x%x data_offset: 0x%x data_size: %d\n",
                    dllname, fpos, hash->local_store_index, xalz.magic, xalz.size, dsc->data_offset, dsc->data_size);
        }

	if (xalz.magic != 0x5a4c4158)
	{
            fprintf(stderr, "Bailing invalid XALZ magic signature found\n");
	    continue;
	}

        if (xalz.size <= 0)
	{
	    // Skip extraction of the file
            fprintf(stderr, "Bailing invalid XALZ payload size value\n");
	    continue;
	}


        list_append(block, list, dllname, xalz.size);

        size_t compressed_file_size = dsc->data_size;
        char* compressed_payload = block_alloc(*block, compressed_file_size);
        if (!compressed_payload)
        {
	    fprintf(stderr, "block_alloc() failed file:%s:%d\n", __FILE__, __LINE__);
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
            	goto EXIT;
			}
        }

        size_t data_size = xalz.size;
        char *data = block_alloc(*block, data_size);
        if (!data)
        {
            fprintf(stderr, "block_alloc() failed file:%s:%d\n", __FILE__, __LINE__);
            goto EXIT;
        }

        int ret =  LZ4_decompress_fast(compressed_payload, data, (int)data_size);
        if (ret <= 0)
        {
            fprintf(stderr, "LZ4 decompression failed\n");
            goto EXIT;
        }

        const char *dir = get_parent_dir(*block, path);
        string_T *output = (dir) ? string_concat(*block, dir, dllname) : string_new(*block, dllname);

        if (!output)
        {
            fprintf(stderr, "string operation failed\n");
            goto EXIT;
        }

        if (dump)
        {
            ret = write_file(output->buffer, data, data_size);
            if (ret <= 0)
            {
                fprintf(stderr, "write_file() failed\n");
                goto EXIT;
            }
        }

    }

EXIT:
    if (file)
    {
        fclose(file);
        file = NULL;
    }

    if (manifest)
    {
        fclose(manifest);
        manifest = NULL;
    }

	return count;
}

